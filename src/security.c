
#include "openssl/ssl.h"
#include "openssl/err.h"

#include "memory.h"
#include "security.h"

#define ssl_panic(err) user_panic("SSL error: \"%s\": %s\n", err, ERR_error_string(ERR_get_error(), NULL));

SSL_CTX* ssl_create_ctx() {
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ctx = SSL_CTX_new(TLSv1_2_server_method());

    if (ctx == NULL) {
        ssl_panic("Could not initialize SSL_CTX");
    }

    SSL_CTX_set_cipher_list(ctx, SSL_CIPHERS);
    SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_RELEASE_BUFFERS);

    return ctx;
}

void ssl_load_certs(SSL_CTX* ctx, const char *cert_file, char *key_file) {
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ssl_panic("Could not load public certificate");
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0){
        ssl_panic("Could not load private key");
    }

    if ( !SSL_CTX_check_private_key(ctx) ) {
        user_panic("Private key does not match the public certificate.");
    }
}

