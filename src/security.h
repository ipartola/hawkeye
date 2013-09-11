#ifndef __SECURITY_H
#define __SECURITY_H

#include "openssl/ssl.h"

#define SSL_CIPHERS "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-RC4-SHA:ECDHE-RSA-AES128-SHA:AES128-SHA:RC4-SHA"

SSL_CTX* ssl_create_ctx();
void ssl_load_certs(SSL_CTX* ctx, const char *cert_file, char *key_file);

#endif
