#ifndef __HTTP_H
#define __HTTP_H

#define INDEX_FILE_NAME "index.html"

struct http_request {
    char *method;
    char *path;
    char *query_string;
    char *protocol;
    char *protocol_version;

    char *host;
    char *authorization;
};

short check_http_auth(char *auth, char *desired_password);
char *get_mime_type(char *filename);
void parse_request(char *lines, struct http_request *req);

#endif
