#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "mime.h"
#include "utils.h"
#include "http.h"

static void normalize_request(char *lines);
static void extract_header(char *line, const char *header, char **val);
static void extract_request_line(char *line, char **method, char **path, char **query_string, char **protocol, char **protocol_version);

// Returns 1 if user is authorized, 0 otherwise.
// Note that if desired_password is NULL, the user is authorized.
short check_http_auth(char *auth, char *desired_password) {
    int i, len;

    if (desired_password == NULL) {
        return 1;
    }

    if (auth == NULL) {
        return 0;
    }

    if (strncasecmp(auth, "Basic", strlen("Basic")) != 0) {
        return 0;
    }

    for (i = strlen("Basic"), len = strlen(auth); is_lws(auth[i]) && i < len; i++) {}

    if (strcmp(&auth[i], desired_password) == 0) {
        return 1;
    }

    return 0;
}

char *get_mime_type(char *filename) {
    char *ext;
    int i;

    for (ext = &filename[strlen(filename) - 1]; ext != filename; ext--) {
        if (*(ext - 1) == '.') {
            break;
        }
    }

    for (i = 0; i < sizeof(MIME_TYPES)/sizeof(MIME_TYPES[0]); i++) {
        if (strcmp(ext, MIME_TYPES[i]) == 0) {
            return (char *) &MIME_TYPES[i][strlen(ext) + 1];
        }
    }

    return "application/octet";
}

static void normalize_request(char *lines) {
    char *c, *c1, *end;
    ssize_t len;

    end = &lines[strstr(lines, "\r\n\r\n") - lines];
    
    // Convert \r\n to \0\n
    for (c = lines; c < end + 3; c++) {
        if (*c == '\r' && *(c + 1) == '\n') {
            *c = '\0';
        }
    }

    // Concat multi-line headers
    for (c = lines; c < end - 2; c++) {
        if (*c == '\0' && *(c + 1) == '\n' && is_lws(*(c + 2))) {
            for (c1 = c + 3; is_lws(*c1); c1++) {}
            len = strlen(c1);
            memmove(c, c1, end - c);
            c[len] = '\0';

            end -= c1 - c;
            c = c1 + len;
        }
    }

}

static void extract_header(char *line, const char *header, char **val) {
    size_t len;
    
    if (*val != NULL) {
        return;
    }

    len = strlen(header);
    
    if (strncasecmp(line, header, len) == 0) {
        *val = line + len;
        trim(*val);
    }

}

// Long and horrible, but it works
static void extract_request_line(char *line, char **method, char **path, char **query_string, char **protocol, char **protocol_version) {
    char *c, *start, *end;

    end = line + strlen(line) - 1;
    start = line;

    for (c = line; c < end; c++) {

        if (*method == NULL) {
            if (is_lws(*c)) {
                *c = '\0';
                *method = start;
                c++;

                for (; is_lws(*c); c++) {}

                start = c;
            }
            continue;
        }

        if (*path == NULL) {
            if (is_lws(*c) || *c == '?') {
                *path = start;

                if (*c != '?') {
                    *query_string = "";
                }
                *c = '\0';
                c++;

                for (; is_lws(*c); c++) {}
                start = c;
            }
        }
        
        if (*query_string == NULL) {
            if (is_lws(*c)) {
                *c = '\0';
                c++;
                *query_string = start;

                for (; is_lws(*c); c++) {}
                break;
            }
        }
        else {
            break;
        }
    }
        
    *protocol = c;
    
    for (; c < end; c++) {
        if (*c == '/') {
            *c = '\0';
            c++;
            *protocol_version = c;
            trim(*protocol_version);
            break;
        }
    }
}

// This function re-uses the request string and modifies it severely.
// Make a copy if you still need it.
void parse_request(char *lines, struct http_request *req) {
    char *line;
    ssize_t len;
    
    req->method = NULL;
    req->path = NULL;
    req->query_string = NULL;
    req->protocol = NULL;
    req->protocol_version = NULL;
    req->host = NULL;
    req->authorization = NULL;
    
    normalize_request(lines);

    line = lines;
    len = strlen(line);
    
    extract_request_line(line, &req->method, &req->path, &req->query_string, &req->protocol, &req->protocol_version);

    line += len + 2;
    len = strlen(line);
    while (len) {
        extract_header(line, "host:", &req->host);
        extract_header(line, "authorization:", &req->authorization);

        line += len + 2;
        len = strlen(line);
    }
}

