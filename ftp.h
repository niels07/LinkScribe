#ifndef FTP_H
#define FTP_H

#include <stdbool.h>

typedef void (*FTPOnConnect)(void *arg, const char *message, bool success);
typedef void (*FTPOnDirList)(void *arg, const char *directory);

typedef struct {
    char host[256];
    char username[256];
    char password[256];
    int port;
    char protocol[10];
    char remote_path[1024];
    FTPOnConnect on_connect;
    FTPOnDirList on_dir_list;
    void *arg;
} FTPConnectData;

typedef struct {
    char id[37]; // UUID string
    char description[256];
    char host[256];
    char local_path[256];
    char remote_path[256];    
    char username[256];
    char password[256];
    int port;
    char protocol[10];
} FTPDetails;

extern void ftp_connect(const FTPDetails *details, FTPOnConnect on_connect, void *arg);
extern void ftp_list_dirs(const FTPDetails *details, FTPOnDirList on_dir_list, void *arg);
extern char* request_ftp_list(FTPDetails *details, const char *path);

#endif
