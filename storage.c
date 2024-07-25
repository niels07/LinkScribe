#include <openssl/evp.h>
#include <openssl/aes.h>
#include <uuid/uuid.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "storage.h"
#include "encryption.h"


#define FILE_PATH "ftp_details.dat" 

void save_ftp_details(const FTPDetails *details) {
    FILE *file = fopen(FILE_PATH, "ab");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }

    FTPDetails encrypted_details = *details;
    encrypt_password(details->password, encrypted_details.password);

    uuid_t binuuid;
    uuid_generate(binuuid);
    uuid_unparse(binuuid, encrypted_details.id);

    fwrite(&encrypted_details, sizeof(FTPDetails), 1, file);
    fclose(file);
}

void load_ftp_details(GList **details_list) {
    FILE *file = fopen(FILE_PATH, "rb");
    if (file == NULL) {
        perror("Failed to open file for reading");
        return;
    }

    FTPDetails *details;
    while ((details = (FTPDetails *)malloc(sizeof(FTPDetails))) != NULL) {
        size_t read = fread(details, sizeof(FTPDetails), 1, file);
        if (read != 1) {
            free(details);
            break;
        }
        printf("password before decryption: %s\n", details->password);
        decrypt_password(details->password, details->password);
        printf("password after decryption: %s\n", details->password);
        *details_list = g_list_append(*details_list, details);
    }

    fclose(file);
}

void update_ftp_details(const FTPDetails *details) {
    GList *details_list = NULL;
    load_ftp_details(&details_list);

    FILE *file = fopen(FILE_PATH, "wb");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }
    
    for (GList *l = details_list; l != NULL; l = l->next) {
        FTPDetails *current = (FTPDetails *)l->data;
        if (g_strcmp0(current->id, details->id) == 0) {
            *current = *details;             
            encrypt_password(current->password, current->password);            
        }
        fwrite(current, sizeof(FTPDetails), 1, file);
    }

    fclose(file);
    g_list_free_full(details_list, g_free);
}

void remove_ftp_details(const char *id) {
    GList *details_list = NULL;
    load_ftp_details(&details_list);

    FILE *file = fopen(FILE_PATH, "wb");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }

    for (GList *l = details_list; l != NULL; l = l->next) {
        FTPDetails *current = (FTPDetails *)l->data;
        if (g_strcmp0(current->id, id) != 0) {
            fwrite(current, sizeof(FTPDetails), 1, file);
        }
        g_free(current);
    }

    fclose(file);
    g_list_free(details_list);
}