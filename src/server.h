
#ifndef SERVER_H
#define SERVER_H

#include <unistd.h>
#include <netinet/in.h>
#include "openssl/ssl.h"

#include "frames.h"
#include "utils.h"

#define MAX_SERVER_SOCKET_BACKLOG 5
#define MAX_REQUEST_HEADER_SIZE 4096
#define SERVER_BUFFER_SIZE 1024*16

#define BOUNDARY "aEjlw7DR5wcqrxG4p12AE0jGIZPlUHyi"

#define STREAM_HEADER "HTTP/1.0 200 OK\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: close\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
    "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
    "Pragma: no-cache\r\n" \
    "Expires: Mon, 1 Jan 2000 00:00:00 GMT\r\n\r\n" \
    "--" BOUNDARY "\r\n"

#define FRAME_HEADER "Content-Type: image/jpeg\r\n\r\n"

#define FRAME_FOOTER "\r\n--" BOUNDARY "\r\n"

#define HTTP_BAD_REQUEST "HTTP/1.1 400 BAD REQUEST\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: close\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "\r\n" \
    "Bad request"

#define HTTP_AUTH_REQUIRED "HTTP/1.1 401 NOT AUTHORIZED\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: close\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "WWW-Authenticate: Basic realm=\"Hawkeye\"\r\n" \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "\r\n" \
    "Password required."

#define HTTP_NOT_FOUND "HTTP/1.1 404 NOT FOUND\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: close\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "\r\n" \
    "Could not find resource at this URL."

#define HTTP_STATIC_FILE_HEADERS_TMPL "HTTP/1.1 200 OK\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: keep-alive\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Content-Length: %ld\r\n" \
    "Content-Type: %s\r\n" \
    "\r\n"

#define HTTP_STREAM_INFO_TEMPLATE "HTTP/1.0 200 OK\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: close\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Content-Type: application/json\r\n" \
    "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
    "Pragma: no-cache\r\n" \
    "Expires: Mon, 1 Jan 2000 00:00:00 GMT\r\n" \
    "\r\n" \
    "{\"stream_count\": %d, \"width\": %d, \"height\": %d}"

#define JPEG_HEADER "HTTP/1.0 200 OK\r\n" \
    "Server: hawkeye\r\n" \
    "Connection: close\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Content-Type: image/jpeg\r\n" \
    "Cache-Control: no-store, no-cache, pre-check=0, post-check=0, max-age=0\r\n" \
    "Pragma: no-cache\r\n" \
    "Expires: Mon, 1 Jan 2000 00:00:00 GMT\r\n\r\n"


#define REQUEST_INCOMPLETE 0
#define REQUEST_STREAM 1
#define REQUEST_NOT_FOUND 2
#define REQUEST_BAD 3
#define REQUEST_STREAM_INFO 4
#define REQUEST_STATIC_FILE 5
#define REQUEST_AUTH_REQUIRED 6
#define REQUEST_STILL 7

#define KEEP_ALIVE_TIMEOUT 30.0

struct client {
    int sock;
    struct sockaddr_storage addr;
    SSL *ssl;

    unsigned long current_frame;
    size_t current_frame_pos;
    double last_communication;

    char request_headers[MAX_REQUEST_HEADER_SIZE];
    size_t request_header_size;
    char request_received;

    size_t resp_pos;
    size_t resp_len;
    char *resp;

    FILE *static_file;

    int request; // If non-negative: index of stream to send. If negative: serve the specific response

    struct frame_buffer *fb;
};

struct server {
    int sock4;
    int sock6;
    SSL_CTX *ssl_ctx;

    struct client *clients[FD_SETSIZE];

    char *stream_info;
    char *auth;
    char *static_root;
};

struct server *create_server(char *host, unsigned short port, struct frame_buffers *fbs, char *static_root, char *auth, char *ssl_cert_file, char *ssl_key_file);
void serve_clients(struct server *s, struct frame_buffers *fbs, double timeout);
void destroy_server(struct server *s);

#endif
