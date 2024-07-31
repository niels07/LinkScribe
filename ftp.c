#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include "ftp.h"
#include "utils.h"

typedef struct {
    CURL *curl;
    FTPConnectData *data;
} DebugData;

struct FtpBuffer {
    char *data;
    size_t size;
};

static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    DebugData *debug_data = (DebugData *)userptr;
    FTPConnectData *connect_data = debug_data->data;

    if (!connect_data) {
        static bool logged_once = false;
        if (!logged_once) {
            PRINT_ERROR("Invalid connect_data or on_connect function.");
            logged_once = true;
        }
        return 0;
    }

    bool pass = strstr(data, "PASS ") == data;
    char *safe_message = NULL;

    if (pass) {
        safe_message = strdup("PASS ********");
    } else {
        safe_message = strndup(data, size);
    }

    if (safe_message) {
        PRINT_MESSAGE("%s", safe_message);
        if (type == CURLINFO_HEADER_OUT && pass) {
            if (connect_data->on_connect) {
                connect_data->on_connect(connect_data->arg, "PASS ********\n", true);
            }
        } else if (type == CURLINFO_TEXT) {
            if (connect_data->on_connect) {
                connect_data->on_connect(connect_data->arg, safe_message, true);
            }
        }
        free(safe_message);
    }

    return 0;
}

static size_t dir_list_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    FTPConnectData *data = (FTPConnectData *)userdata;
    char *line = malloc(size * nmemb + 1);
    if (!line) {
        PRINT_ERROR("Memory allocation failure");
        return 0;
    }
    memcpy(line, ptr, size * nmemb);
    line[size * nmemb] = '\0';
    PRINT_MESSAGE("DIR LIST CALLBACK: Directory entry: %s", line);

    if (g_utf8_validate(line, -1, NULL)) {
        if (data->on_dir_list) {
            data->on_dir_list(data->arg, line);
        }
    } else {
        PRINT_ERROR("Invalid UTF-8 string received: %s", line);
    }

    free(line);
    return size * nmemb;
}

static size_t write_callback(void *buffer, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct FtpBuffer *mem = (struct FtpBuffer *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if(ptr == NULL) {
        // Out of memory
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), buffer, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}
static void* ftp_connect_thread(void *arg) {
    FTPConnectData *data = (FTPConnectData *)arg;
    if (!data) {
        PRINT_ERROR("Invalid data.");
        return NULL;
    }

    char url[280];
    snprintf(url, sizeof(url), "%s://%s:%d", data->protocol, data->host, data->port);
    PRINT_MESSAGE("Connecting to %s", url);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) {
        PRINT_ERROR("Failed to initialize curl");
        if (data->on_connect) {
            data->on_connect(data->arg, "Failed to initialize CURL.", false);
        }
        free(data);
        return NULL;
    }

    DebugData debug_data = {curl, data};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, data->username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, data->password);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &debug_data);

    if (strcmp(data->protocol, "ftps") == 0 || strcmp(data->protocol, "ftpes") == 0) {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        PRINT_MESSAGE("Connection established");
        char *remote_path;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &remote_path);
        if (data->on_connect) {
            data->on_connect(data->arg, remote_path, true);
        }
    } else {
        const char *error = curl_easy_strerror(res);
        PRINT_ERROR("Connection failed: %s", error);
        if (data->on_connect) {
            data->on_connect(data->arg, error, false);
        }
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    free(data);
    return NULL;
}

static void* ftp_list_dirs_thread(void *arg) {
    FTPConnectData *data = (FTPConnectData *)arg;
    if (!data) {
        PRINT_ERROR("Invalid data.");
        return NULL;
    }

    char url[2048];
    snprintf(url, sizeof(url), "%s://%s:%d%s", data->protocol, data->host, data->port, data->remote_path);
    PRINT_MESSAGE("Requesting directory list from %s", url);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) {
        PRINT_ERROR("Failed to initialize CURL.");
        free(data);
        return NULL;
    }

    DebugData debug_data = {curl, data};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, data->username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, data->password);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dir_list_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &debug_data);

    if (strcmp(data->protocol, "ftps") == 0 || strcmp(data->protocol, "ftpes") == 0) {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "LIST");
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPRT, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        PRINT_MESSAGE("Directory list retrieved successfully.");
        if (data->on_dir_list) {
            data->on_dir_list(data->arg, "Directory list retrieved successfully.");
        }
    } else {
        const char *error = curl_easy_strerror(res);
        PRINT_ERROR("Directory list retrieval failed: %s", error);
        if (data->on_dir_list) {
            data->on_dir_list(data->arg, error);
        }
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    free(data);
    return NULL;
}

void ftp_connect(const FTPDetails *details, FTPOnConnect on_connect, void *arg) {
    FTPConnectData *data = (FTPConnectData *)malloc(sizeof(FTPConnectData));
    if (!data) {
        PRINT_ERROR("Memory allocation failed.");
        return;
    }

    strncpy(data->host, details->host, sizeof(data->host) - 1);
    strncpy(data->username, details->username, sizeof(data->username) - 1);
    strncpy(data->password, details->password, sizeof(data->password) - 1);
    data->port = details->port;
    strncpy(data->protocol, details->protocol, sizeof(data->protocol) - 1);
    data->on_connect = on_connect;
    data->on_dir_list = NULL;
    data->arg = arg;

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, ftp_connect_thread, data) != 0) {
        PRINT_ERROR("Failed to create thread.");
        free(data);
        return;
    }
    pthread_detach(thread_id);
}

void ftp_list_dirs(const FTPDetails *details, FTPOnDirList on_dir_list, void *arg) {
    FTPConnectData *data = (FTPConnectData *)malloc(sizeof(FTPConnectData));
    if (!data) {
        PRINT_ERROR("Memory allocation failed.");
        return;
    }
    memset(data, 0, sizeof(FTPConnectData));
    strncpy(data->host, details->host, sizeof(data->host) - 1);
    strncpy(data->username, details->username, sizeof(data->username) - 1);
    strncpy(data->password, details->password, sizeof(data->password) - 1);
    data->port = details->port;
    strncpy(data->protocol, details->protocol, sizeof(data->protocol) - 1);
    strncpy(data->remote_path, details->remote_path, sizeof(data->remote_path) - 1);
    data->on_connect = NULL;
    data->on_dir_list = on_dir_list;
    data->arg = arg;

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, ftp_list_dirs_thread, data) != 0) {
        PRINT_ERROR("Failed to create thread.");
        free(data);
        return;
    }
    pthread_detach(thread_id);
}

char* request_ftp_list(FTPDetails *details, const char *path) {
    CURL *curl;
    CURLcode res;
    struct FtpBuffer ftpbuf = { .data = NULL, .size = 0 };

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
        char url[768];
        snprintf(url, sizeof(url), "ftp://%s:%d%s", details->host, details->port, path);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERNAME, details->username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, details->password);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftpbuf);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(ftpbuf.data);
            ftpbuf.data = NULL;
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return ftpbuf.data;
}
