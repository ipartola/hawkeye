#ifndef __MEMORY_H
#define __MEMORY_H

#define NONCE_SIZE 64

void user_panic(const char* fmt, ...);
void panic(const char* str);
void rng_init();
void rng_cleanup();
unsigned int xrandom();
char *make_nonce();

#endif
