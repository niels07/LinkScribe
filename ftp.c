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
    size_t total_size = size * nmemb;
    char *line = malloc(total_size + 1);
    if (!line) {
        PRINT_ERROR("Memory allocation failure");
        return 0;
    }
    memcpy(line, ptr, total_size);
    line[total_size] = '\0';
    PRINT_MESSAGE("DIR LIST CALLBACK: Directory entry: %s", line);
    if (data->on_dir_list) {
        data->on_dir_list(data->arg, line);
    }
    free(line);
    return total_size;
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

    char url[280];
    snprintf(url, sizeof(url), "%s://%s:%d", data->protocol, data->host, data->port);
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

    // Increase timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L); // Increase connection timeout to 60 seconds
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // Increase overall operation timeout to 120 seconds
    curl_easy_setopt(curl, CURLOPT_FTP_RESPONSE_TIMEOUT, 30L); // Set FTP response timeout to 30 seconds

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "NLST"); // Explicitly set NLST command for FTP

    // Enable passive mode
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 1L); // Enable EPSV mode
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPRT, 1L); // Enable EPRT mode

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
    memset(data, 0, sizeof(FTPConnectData)); // Ensure all fields are initialized
    strncpy(data->host, details->host, sizeof(data->host) - 1);
    strncpy(data->username, details->username, sizeof(data->username) - 1);
    strncpy(data->password, details->password, sizeof(data->password) - 1);
    data->port = details->port;
    strncpy(data->protocol, details->protocol, sizeof(data->protocol) - 1);
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
