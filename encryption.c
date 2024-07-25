#include "encryption.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define KEY_SIZE 32
#define IV_SIZE 16
#define KEY_FILE "data/key.bin"
#define IV_FILE "data/iv.bin"

static bool file_exists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

bool generate_and_save_key_iv(void) {
    unsigned char key[KEY_SIZE];
    unsigned char iv[IV_SIZE];

    if (!file_exists(KEY_FILE)) {
        if (RAND_bytes(key, KEY_SIZE) != 1) {
            fprintf(stderr, "Error generating random key\n");
            return false;
        }
    
        FILE *key_fp = fopen(KEY_FILE, "wb");
        if (!key_fp) {
            perror("Failed to open key file for writing");
            return false;
        }
        fwrite(key, 1, KEY_SIZE, key_fp);
        fclose(key_fp);
    }

    if (!file_exists(IV_FILE)) {
        if (RAND_bytes(iv, IV_SIZE) != 1) {
            fprintf(stderr, "Error generating random IV\n");
            return false;
        }
        FILE *iv_fp = fopen(IV_FILE, "wb");
        if (!iv_fp) {
            perror("Failed to open IV file for writing");
            return false;
        }
        fwrite(iv, 1, IV_SIZE, iv_fp);
        fclose(iv_fp);
    }
    return true;
}

static bool load_key_iv(unsigned char *key, unsigned char *iv) {
    FILE *key_fp = fopen(KEY_FILE, "rb");
    if (!key_fp) {
        perror("Failed to open key file for reading");
        return false;
    }
    fread(key, 1, KEY_SIZE, key_fp);
    fclose(key_fp);

    FILE *iv_fp = fopen(IV_FILE, "rb");
    if (!iv_fp) {
        perror("Failed to open IV file for reading");
        return false;
    }
    fread(iv, 1, IV_SIZE, iv_fp);
    fclose(iv_fp);
    return true;
}

bool encrypt_password(const char *plaintext, char *encrypted) {
    unsigned char key[KEY_SIZE];
    unsigned char iv[IV_SIZE];
    if (!load_key_iv(key, iv)) {
        return false;
    }

    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;

    ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    EVP_EncryptUpdate(ctx, (unsigned char *)encrypted, &len, (unsigned char *)plaintext, strlen(plaintext));
    ciphertext_len = len;

    EVP_EncryptFinal_ex(ctx, (unsigned char *)encrypted + len, &len);
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return true;
}

bool decrypt_password(const char *encrypted, char *decrypted) {
    unsigned char key[KEY_SIZE];
    unsigned char iv[IV_SIZE];
    if (!load_key_iv(key, iv)) {
        return false;
    }

    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;

    ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    EVP_DecryptUpdate(ctx, (unsigned char *)decrypted, &len, (unsigned char *)encrypted, strlen(encrypted));
    plaintext_len = len;

    EVP_DecryptFinal_ex(ctx, (unsigned char *)decrypted + len, &len);
    plaintext_len += len;

    decrypted[plaintext_len] = '\0';

    EVP_CIPHER_CTX_free(ctx);
    return true;
}