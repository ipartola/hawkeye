
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits.h>
#include <sys/stat.h>
#include "openssl/ssl.h"
#include "openssl/err.h"

#include "frames.h"
#include "memory.h"
#include "logger.h"
#include "utils.h"
#include "http.h"
#include "security.h"

#include "server.h"

#define serrchk(err) {\
    if (errno == EINTR) {\
        return;\
    }\
    else {\
        panic(err);\
    }\
}

static char* ntop(struct sockaddr_storage *addr, char *cbuf, size_t cbuf_len);
static int open_sock(const char *hostname, unsigned short port, int family, int socktype);
static void add_client(struct server *s, int sock, struct sockaddr_storage *addr, struct frame_buffers *fbs);
static void remove_client(struct server *s, struct client *c);
static void reset_client(struct client *c);
static size_t client_read(struct client *c, void *buf, const size_t len);
static ssize_t client_write(struct client *c, const void *buf, const size_t len);
static void read_request(struct server *s, struct client *c, struct frame_buffers *fbs);
static void handle_request(struct server *s, struct client *c, struct frame_buffers *fbs);
static void respond_to_client(struct server *s, struct client *c);

static char* ntop(struct sockaddr_storage *addr, char *cbuf, size_t cbuf_len) {
    switch (addr->ss_family) {
        case AF_INET6:
            inet_ntop(addr->ss_family, &((struct sockaddr_in6 *) addr)->sin6_addr, cbuf, cbuf_len);
            return cbuf;
        case AF_INET:
            inet_ntop(addr->ss_family, &((struct sockaddr_in *) addr)->sin_addr, cbuf, cbuf_len);
            return cbuf;
        default:
            strncpy(cbuf, "unknown (unsupported address family)", cbuf_len);
            return cbuf;
    }
}

static int open_sock(const char *hostname, unsigned short port, int family, int socktype) {
    struct addrinfo hints, *res, *ressave;
    int n, sock;
    int sockoptval = 1;
    char cbuf[INET6_ADDRSTRLEN]; // general purpose buffer for various string conversions in this function

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags    = AI_PASSIVE;
    hints.ai_family   = family;
    hints.ai_socktype = socktype;

    snprintf(cbuf, sizeof(cbuf), "%d", port);
    n = getaddrinfo(strlen(hostname) ? hostname : NULL, cbuf, &hints, &res);

    if (n < 0) {
        log_itf(LOG_DEBUG, "getaddrinfo error for hostname '%s': %s.", hostname, gai_strerror(n));
        return -1;
    }

    ressave = res;

    // Try open socket with each address getaddrinfo returned,
    // until getting a valid listening socket.
    
    sock = -1;
    while (res) {
        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));

        if (sock >= 0) {
            if (bind(sock, res->ai_addr, res->ai_addrlen) == 0) {
                if (res->ai_family == AF_INET6) {
                    inet_ntop(res->ai_family, (void *) &((struct sockaddr_in6*) res->ai_addr)->sin6_addr, cbuf, sizeof(cbuf));
                    log_itf(LOG_INFO, "Listening on %s port %d.", cbuf, port);
                }
                else if (res->ai_family == AF_INET) {
                    inet_ntop(res->ai_family, (void *) &((struct sockaddr_in*) res->ai_addr)->sin_addr, cbuf, sizeof(cbuf));
                    log_itf(LOG_INFO, "Listening on %s port %d.", cbuf, port);
                }
                else {
                    log_it(LOG_INFO, "Not sure what address I am listening on.");
                }
                

                break;
            }

            close(sock);
            sock = -1;
        }
        res = res->ai_next;
    }

    if (sock >= 0) {
        if (listen(sock, MAX_SERVER_SOCKET_BACKLOG) < 0) {
            panic("listen() failed.");
        }
    }

    freeaddrinfo(ressave);

    return sock;
}

static ssize_t client_write(struct client *c, const void *buf, const size_t len) {
    if (c->ssl != NULL) {
        return SSL_write(c->ssl, buf, len);
    }
    else {
        return send(c->sock, buf, len, 0);
    }
}

static size_t client_read(struct client *c, void *buf, const size_t len) {
    if (c->ssl != NULL) {
        return SSL_read(c->ssl, buf, len);
    }
    else {
        return recv(c->sock, buf, len, 0);
    }
}

static void add_client(struct server *s, int sock, struct sockaddr_storage *addr, struct frame_buffers *fbs) {
    struct client *c;
    char cbuf[INET6_ADDRSTRLEN]; // general purpose buffer for various string conversions in this function

    c = malloc(sizeof(struct client));
    memset(c, 0, sizeof(struct client));

    c->sock = sock;
    memcpy(&c->addr, addr, sizeof(struct sockaddr_storage));
    c->last_communication = gettime();
    c->current_frame = 0;
    c->current_frame_pos = 0;
    c->request_header_size = 0;
    c->request = REQUEST_INCOMPLETE;
    c->resp = NULL;
    c->resp_pos = 0;
    c->resp_len = 0;
    c->fb = NULL;
    c->static_file = NULL;

    if (s->ssl_ctx != NULL) {
        c->ssl = SSL_new(s->ssl_ctx);
        SSL_set_fd(c->ssl, c->sock);

        if (SSL_accept(c->ssl) < 1) {
            log_itf(LOG_ERROR, "Error occured doing the SSL handshake: %s", ERR_error_string(ERR_get_error(), NULL));
            free(c);
            return;
        }
    }

    s->clients[c->sock] = c;

    log_itf(LOG_INFO, "Client connected from %s.", ntop(&c->addr, cbuf, sizeof(cbuf)));
}

static void remove_client(struct server *s, struct client *c) {
    char cbuf[INET6_ADDRSTRLEN];
    log_itf(LOG_INFO, "Disconneting client from %s.", ntop(&c->addr, cbuf, sizeof(cbuf)));
    
    s->clients[c->sock] = NULL;
    close(c->sock);
    
    if (c->static_file != NULL) {
        fclose(c->static_file);
    }

    if (c->resp != NULL) {
        free(c->resp);
    }
    
    if (c->ssl) {
        SSL_free(c->ssl);
    }

    free(c);
}

static void reset_client(struct client *c) {
    c->request_header_size = 0;
    c->request = REQUEST_INCOMPLETE;
    c->request_received = 0;
    c->resp_pos = 0;
    c->resp_len = 0;
    
    if (c->static_file != NULL) {
        fclose(c->static_file);
    }
    c->static_file = NULL;

    if (c->resp != NULL) {
        free(c->resp);
    }
    c->resp = NULL;
}

static void read_request(struct server *s, struct client *c, struct frame_buffers *fbs) {
    size_t len;
    char buf[SERVER_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));

    if (!c->request_received) {

        if ((len = client_read(c, buf, sizeof(buf))) < 1) {
            if (errno == EINTR) {
                return;
            }

            // len == 0 means that the client has disconnected
            // len == -1 means an error occured
            return remove_client(s, c);
        }
        c->last_communication = gettime();

        // Check if we have the whole thing
        strncpy(&c->request_headers[c->request_header_size], buf, MAX_REQUEST_HEADER_SIZE - c->request_header_size);
        c->request_header_size += len;

        if (strstr(c->request_headers, "\r\n\r\n") != NULL) {
            c->request_received = 1;
        }
    }

    // Parse request headers
    if (c->request_received && c->request == REQUEST_INCOMPLETE) {
        handle_request(s, c, fbs);
    }
}

static void set_client_response(struct client *c, int request, char *response) {
    c->request = request;
    c->resp = strdup(response);
    c->resp_len = strlen(response);
}

static void handle_request(struct server *s, struct client *c, struct frame_buffers *fbs) {
    int index;
    char cbuf[INET6_ADDRSTRLEN]; // general purpose buffer for various string conversions in this function
    char tmp_filename[PATH_MAX + 1], filename[PATH_MAX + 1]; // Need 2 of these for realpath()
    char resp_head[sizeof(HTTP_STATIC_FILE_HEADERS_TMPL) + 256];
    struct http_request req;

    parse_request(c->request_headers, &req);
    if (req.protocol_version == NULL) {
        set_client_response(c, REQUEST_BAD, HTTP_BAD_REQUEST);
        return;
    }

    log_itf(LOG_INFO, "Client at %s requested %s %s%s%s.", 
        ntop(&c->addr, cbuf, sizeof(cbuf)),
        req.method,
        req.path,
        *req.query_string != '\0' ? "?" : "",
        req.query_string
    );
    
    if (strcmp(req.method, "GET") != 0 || strcmp(req.protocol, "HTTP") != 0) {
        return set_client_response(c, REQUEST_BAD, HTTP_BAD_REQUEST);
    }

    if (strncmp(req.path, "/img/", 5) != 0) {
        if (!check_http_auth(req.authorization, s->auth)) {
            return set_client_response(c, REQUEST_AUTH_REQUIRED, HTTP_AUTH_REQUIRED);
        }
    }

    // /stream/
    if (strncmp(req.path, "/stream/", strlen("/stream/")) == 0) {
        if (strcmp(req.path, "/stream/info") == 0) {
            set_client_response(c, REQUEST_STREAM_INFO, s->stream_info);
        }
        else {
            // /stream/0, /stream/1, etc
            index = atoi(&req.path[strlen("/stream/")]);

            if (index < 0 || index >= fbs->count) {
                set_client_response(c, REQUEST_NOT_FOUND, HTTP_NOT_FOUND);
            }
            else {
                set_client_response(c, REQUEST_STREAM, STREAM_HEADER);
                
                c->fb = &fbs->buffers[index];
                c->current_frame = c->fb->current_frame;
                c->current_frame_pos = 0;
            }
        }
    }
    else if (strncmp(req.path, "/still/", strlen("/still/")) == 0) {
        index = atoi(&req.path[strlen("/still/")]);
        if (index < 0 || index >= fbs->count) {
            set_client_response(c, REQUEST_NOT_FOUND, HTTP_NOT_FOUND);
        }
        else {
            set_client_response(c, REQUEST_STILL, JPEG_HEADER);

            c->fb = &fbs->buffers[index];
            c->current_frame = c->fb->current_frame;
            c->current_frame_pos = 0;
        }
    }
    else {

        if (!strlen(s->static_root)) {
            return set_client_response(c, REQUEST_NOT_FOUND, HTTP_NOT_FOUND);
        }
                
        // Serving static file
        strncpy(tmp_filename, s->static_root, PATH_MAX);
        strncat(tmp_filename, req.path, PATH_MAX);
        if (tmp_filename[strlen(tmp_filename) - 1] == '/') {
            strncat(tmp_filename, INDEX_FILE_NAME, PATH_MAX);
        }

        // This also checks if the file exists
        if (NULL == realpath(tmp_filename, filename) ||
            strncmp(filename, s->static_root, strlen(s->static_root)) != 0) {
                return set_client_response(c, REQUEST_NOT_FOUND, HTTP_NOT_FOUND);

        }

        // Fill in response template
        snprintf(resp_head, sizeof(resp_head), HTTP_STATIC_FILE_HEADERS_TMPL,
            (long) file_size(filename),
            get_mime_type(filename)
        );

        set_client_response(c, REQUEST_STATIC_FILE, resp_head);
        c->static_file = fopen(filename, "rb");
    }
    
}

static void respond_to_client(struct server *s, struct client *c) {
    struct frame *f;
    ssize_t len;
    char buf[SERVER_BUFFER_SIZE];
    
    if (!c->request_received || c->request == REQUEST_INCOMPLETE) {
        return;
    }
    
    // Serve response in the client's resp buffer
    if (c->resp_pos < c->resp_len - 1) {
        if ((len = client_write(c, &c->resp[c->resp_pos], c->resp_len - c->resp_pos)) > 0) {
            c->resp_pos += len;
        }
        c->last_communication = gettime();
        return;
    }
    
    if (c->request == REQUEST_STILL) {
        if ((f = get_frame(c->fb, c->current_frame)) == NULL) {
            // Client failed to read the frame fast enough before it disappeared
            return remove_client(s, c);
        }

        f = get_frame(c->fb, c->current_frame);
        ssize_t jpeg_len = f->data_len - (strlen(FRAME_HEADER) + strlen(FRAME_FOOTER));
        char *jpeg_ptr = f->data + strlen(FRAME_HEADER);
        len = client_write(c, jpeg_ptr, jpeg_len - c->current_frame_pos);

        if (len < 0) {
            if (errno == EINTR) {
                return;
            }
            return remove_client(s, c);
        }
        c->last_communication = gettime();
        c->current_frame_pos += len;
        
        if (c->current_frame_pos == jpeg_len) {
            return remove_client(s, c);
        }
        return;
    }
    // Serve static file
    else if (c->request == REQUEST_STATIC_FILE) {
        size_t flen;

        if ((flen = fread(buf, sizeof(char), sizeof(buf), c->static_file)) < 1) {
            // File is done, reset client for next request
            if (feof(c->static_file)) {
                reset_client(c);
            }

            return;
        }
        
        if ((len = client_write(c, buf, flen)) < 0) {
            if (errno == EINTR) {
                fseek(c->static_file, len, SEEK_CUR); // Rewind, as if we never read the file
                return;
            }
            return remove_client(s, c);
        }
        c->last_communication = gettime();

        if (len < flen) {
            fseek(c->static_file, len - flen, SEEK_CUR); // Rewind by how much of the file we did not send
        }

        return;
    }
    // Serve stream
    else if (c->request == REQUEST_STREAM) {
        
        if ((f = get_frame(c->fb, c->current_frame)) == NULL) {
            // The client is very far behind. Catch them up
            c->current_frame = c->fb->current_frame;
            c->current_frame_pos = 0;
            f = get_frame(c->fb, c->current_frame);

            if (f == NULL) {
                return; // This frame literally does not exist yet
            }
        }

        if (c->current_frame < c->fb->current_frame || (c->current_frame == c->fb->current_frame && c->current_frame_pos < f->data_len)) {

            len = client_write(c, &f->data[c->current_frame_pos], f->data_len - c->current_frame_pos);

            if (len < 0) {
                if (errno == EINTR) {
                    return;
                }
                return remove_client(s, c);
            }
            c->last_communication = gettime();

            c->current_frame_pos += len;
        }

        if (c->current_frame_pos == f->data_len && c->current_frame < c->fb->current_frame) {
            // We have already finished the current frame, and a new frame is available
            c->current_frame = c->fb->current_frame;
            c->current_frame_pos = 0;
        }

    }
    // Response was just in the c->resp buffer, so we are done
    else {
        return remove_client(s, c);
    }

}

struct server *create_server(char *host, unsigned short port, struct frame_buffers *fbs, char *static_root, char *auth, char *ssl_cert_file, char *ssl_key_file) {
    struct server *s = malloc(sizeof(struct server));
    size_t stream_info_buf_size;
    
    memset(s->clients, 0, sizeof(s->clients));

    s->sock6 = open_sock(host, port, AF_INET6, SOCK_STREAM);
    s->sock4 = open_sock(host, port, AF_INET, SOCK_STREAM);
    
    if (s->sock4 < 0 && s->sock6 < 0) {
        panic("Could not bind to socket.");
    }

    stream_info_buf_size = sizeof(HTTP_STREAM_INFO_TEMPLATE) + 64; // Should be enough room for the data
    s->stream_info = malloc(stream_info_buf_size); 
    snprintf(s->stream_info,
            stream_info_buf_size,
            HTTP_STREAM_INFO_TEMPLATE,
            (int) fbs->count,
            fbs->buffers[0].vd->width,
            fbs->buffers[0].vd->height
    );

    s->auth = NULL;
    if (strlen(auth)) {
        s->auth = base64_encode((unsigned char *) auth);
    }

    s->static_root = strdup(static_root);

    s->ssl_ctx = NULL;

    if (strlen(ssl_cert_file) && strlen(ssl_key_file)) {
        s->ssl_ctx = ssl_create_ctx();
        ssl_load_certs(s->ssl_ctx, ssl_cert_file, ssl_key_file);
    }
    else if (strlen(auth)) {
        log_itf(LOG_WARNING, "You are password protecting your streams, but not using encryption.\nAnyone can see your password!");
    }

    return s;
}

void destroy_server(struct server *s) {
    int i;
    struct client *c;

    for (i = 0; i < FD_SETSIZE; i++) {
        c = s->clients[i];
        if (c != NULL) {
            remove_client(s, c);
        }
    }

    if (s->sock4 >= 0) {
        close(s->sock4);
    }

    if (s->sock6 >= 0) {
        close(s->sock6);
    }
    
    if (s->ssl_ctx != NULL) {
        SSL_CTX_free(s->ssl_ctx);
    }

    free(s->auth);
    free(s->static_root);
    free(s->stream_info);

    free(s);
}

void serve_clients(struct server *s, struct frame_buffers *fbs, double timeout) {
	fd_set read_set, write_set;
	int select_result, sock, client_sock, highest_sock_num = max(s->sock4, s->sock6);
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct client *c;
    struct timeval timeout_tv;
    double now = gettime();

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    if (s->sock4 >= 0) {
        FD_SET(s->sock4, &read_set);
    }

    if (s->sock6 >= 0) {
        FD_SET(s->sock6, &read_set);
    }

    for (sock = 0; sock < FD_SETSIZE; sock++) {
        c = s->clients[sock];
        if (c != NULL) {
            FD_SET(c->sock, &read_set);
            FD_SET(c->sock, &write_set);
            highest_sock_num = max(highest_sock_num, c->sock);
        }
    }

    double_to_timeval(timeout, &timeout_tv);
    if ((select_result = select(highest_sock_num + 1, &read_set, &write_set, NULL, &timeout_tv)) < 0) {
        serrchk("select() failed");
    }

    if (!select_result) {
        return;
    }

    for (sock = 0; sock <= highest_sock_num; sock++) {

        // Look over read_set
        if (FD_ISSET(sock, &read_set)) {
            if (sock == s->sock4 || sock == s->sock6) {
                // Accept client
                if ((client_sock = accept(sock, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
                    serrchk("accept() failed");
                }

                add_client(s, client_sock, &client_addr, fbs);
                continue;
            }
            else {
                c = s->clients[sock];
                if (c != NULL) {
                    read_request(s, c, fbs);
                }
            }
        }

        // Look over write set
        if (FD_ISSET(sock, &write_set)) {
            c = s->clients[sock];
            if (c != NULL) {
                respond_to_client(s, c);
            }
        }

        // Prune clients that are not communicating
        if ((c = s->clients[sock]) != NULL) {
            if (now - c->last_communication > KEEP_ALIVE_TIMEOUT) {
                remove_client(s, c);
            }
        }

    }
}

