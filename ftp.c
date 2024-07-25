#include <curl/curl.h>
#include <pthread.h>
#include "ftp.h"

static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    GtkWidget *output_text_view = (GtkWidget *)userptr;
    if (type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT || type == CURLINFO_DATA_IN || type == CURLINFO_DATA_OUT) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_text_view));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(buffer, &end);
        gtk_text_buffer_insert(buffer, &end, data, size);
    }
    return 0;
}

static void* ftp_connect_thread(void *arg) {
    FTPConnectData *data = (FTPConnectData *)arg;
    char url[280];
    snprintf(url, sizeof(url), "%s://%s:%d", data->protocol, data->host, data->port);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) {
        g_free(data);
        return NULL;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, data->username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, data->password);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, data->output_text_view);

    if (strcmp(data->protocol, "ftps") == 0 || strcmp(data->protocol, "ftpes") == 0) {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    if (curl_easy_perform(curl) == CURLE_OK) {
        char *remote_path;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &remote_path);
        g_idle_add((GSourceFunc)gtk_list_store_set, data->store);
        gtk_list_store_set(data->store, &data->iter, 6, remote_path, 7, "Connected", -1);
    } else {
        g_idle_add((GSourceFunc)gtk_list_store_set, data->store);
        gtk_list_store_set(data->store, &data->iter, 7, "Connection Failed", -1);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    g_free(data);
    return NULL;
}

gboolean ftp_connect(const FTPDetails *details, GtkWidget *output_text_view, GtkTreeIter iter, GtkListStore *store) {
    FTPConnectData *data = g_new0(FTPConnectData, 1);
    strncpy(data->host, details->host, sizeof(data->host) - 1);
    strncpy(data->username, details->username, sizeof(data->username) - 1);
    strncpy(data->password, details->password, sizeof(data->password) - 1);
    data->port = details->port;
    strncpy(data->protocol, details->protocol, sizeof(data->protocol) - 1);
    data->output_text_view = output_text_view;
    data->iter = iter;
    data->store = store;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, ftp_connect_thread, data);
    pthread_detach(thread_id);

    return TRUE;
}
