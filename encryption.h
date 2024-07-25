#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <openssl/evp.h>
#include <string.h>
#include <stdbool.h>

extern bool encrypt_password(const char *plaintext, char *encrypted);
extern bool decrypt_password(const char *encrypted, char *decrypted);
extern bool generate_and_save_key_iv(void);

#endif