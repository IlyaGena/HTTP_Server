//#include <sys/types.h>
//#include <sys/select.h>
//#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
//#include <time.h>
#include <fcntl.h>
#include <microhttpd.h>

#define PORT    8888
#define POST    1
#define GET     0
#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512

#define pass    "qwerty"
#define user    "qwerty"

const char* mime_type_img = "image/png";
const char* mime_type_text = "text/html";

const char* path_content = "/content";

const char *errorServerPage =
        "<html><body><h1>Sorry. Server Error!</h1></body></html>";
const char *errorFilePage =
        "<html><body>Server error! Don't read File!</body></html>";
const char *errorFileAuthPage =
        "<html><body>Server error! You can't read this text!</body></html>";

struct connection_info_struct
{
    int connectiontype;
    struct MHD_PostProcessor *postprocessor;
    int answercode;

    struct MHD_Connection *ptr_connection;

    const char *pathfile;
    int checkUsername;
    int checkPass;
};

static MHD_Result send_file (struct MHD_Connection *connection,
                             const struct stat *buffer,
                             int* fileDescriptor,
                             int status_code,
                             const char mimetype)
{
    MHD_Result ret;
    struct MHD_Response* response;

    response = MHD_create_response_from_fd_at_offset64(buffer->st_size, *fileDescriptor, 0);

    printf("Data: %ld \n", buffer->st_size);

    if (!response)
        return MHD_NO;

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, &mimetype);

    ret = MHD_queue_response (connection, status_code, response);
    MHD_destroy_response (response);

    printf("Result: %d\n", ret);

    return ret;
}

static MHD_Result errorFile(struct MHD_Connection *connection,
                            const char* errorPage,
                            int* fileDescriptor)
{
    MHD_Result result;
    struct MHD_Response *response;

    /* error accessing file */
    if (*fileDescriptor != -1)
        close (*fileDescriptor);

    response = MHD_create_response_from_buffer (strlen (errorPage),
                                                (void *) errorPage,
                                                MHD_RESPMEM_PERSISTENT);

    if (NULL != response)
    {
        result = MHD_queue_response (connection,
                                     MHD_HTTP_INTERNAL_SERVER_ERROR,
                                     response);

        MHD_destroy_response (response);

        return result;
    }
    else
        return MHD_NO;
}

static MHD_Result sendPage(struct MHD_Connection *connection,
                            const char* errorPage)
{
    MHD_Result result;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer (strlen (errorPage),
                                                (void *) errorPage,
                                                MHD_RESPMEM_PERSISTENT);

    if (NULL != response)
    {
        result = MHD_queue_response (connection,
                                     MHD_HTTP_INTERNAL_SERVER_ERROR,
                                     response);

        MHD_destroy_response (response);

        return result;
    }
    else
        return MHD_NO;

}

static MHD_Result answer_to_get_request (struct MHD_Connection *connection,
                                         const char *url)
{
    int filedescriptor = 0;
    struct stat buffer;

    int len = strlen(get_current_dir_name()) + strlen(path_content);
    char filePath[len];
    strcpy(filePath, get_current_dir_name());
    strcat(filePath, path_content);
    printf("Path dir: %s\n", filePath);

    // ответ на /
    if (0 == strcmp(url, "/"))
    {
        strcat(filePath, "/index");
        printf("Path file: %s\n", filePath);

        if ( (-1 == (filedescriptor = open (filePath, O_RDONLY))) || (0 != fstat (filedescriptor, &buffer)) )
        {
            printf("Error read!\n");
            return errorFile(connection, errorFilePage, &filedescriptor);
        }
        printf("Success read!\n");
        return send_file(connection, &buffer, &filedescriptor, MHD_HTTP_OK, *mime_type_text);
    }

    // ответ на все остальное
    strcat(filePath, url);
    printf("Path file: %s\n", filePath);

    if ( (-1 == (filedescriptor = open (filePath, O_RDONLY))) || (0 != fstat (filedescriptor, &buffer)) )
    {
        printf("Error read!\n");
        return errorFile(connection, errorFilePage, &filedescriptor);
    }

    printf("Success read!\n");
    return send_file(connection, &buffer, &filedescriptor, MHD_HTTP_OK, *mime_type_text);
}

static MHD_Result answer_to_post_request (struct MHD_Connection *connection,
                                          struct connection_info_struct *con_info,
                                          const char *url,
                                          const char *upload_data,
                                          size_t *upload_data_size)
{
    // вызов пост процессора
    if (*upload_data_size != 0)
    {
        printf("URL POST: %s\n", url);
        MHD_post_process(con_info->postprocessor, upload_data,
                          *upload_data_size);
        *upload_data_size = 0;

        return MHD_YES;
    }
    else if (NULL != con_info->pathfile)
    {
        printf("Answer to post\n");
        printf("Path: %s\n", con_info->pathfile);

        int filedescriptor = 0;
        struct stat buffer;

        int len = strlen(get_current_dir_name()) + strlen(con_info->pathfile);
        char path[len];
        strcpy(path, get_current_dir_name());
        strcat(path, con_info->pathfile);

        if ( (-1 == (filedescriptor = open(path, O_RDONLY))) || (0 != fstat (filedescriptor, &buffer)) )
        {
            printf("Error read! %s\n", path);
            return errorFile(connection, errorFilePage, &filedescriptor);
        }
        printf("Success read!\n");
        return send_file(connection, &buffer, &filedescriptor, MHD_HTTP_OK, *mime_type_text);
    }
    else if(!con_info->checkPass && !con_info->checkUsername)
    {
        printf("Bad Password!\n");
        return sendPage(connection, errorFileAuthPage);
    }

    return MHD_NO;
}

static MHD_Result iterate_post (void *coninfo_cls,
                                enum MHD_ValueKind kind,
                                const char *key,
                                const char *filename,
                                const char *content_type,
                                const char *transfer_encoding,
                                const char *data,
                                uint64_t off,
                                size_t size)
{
    printf("iterate_post\n");
    struct connection_info_struct *con_info = static_cast<connection_info_struct*>(coninfo_cls);

    printf("kind: %u\n", kind);
    printf("key: %s\n", key);
    printf("filename: %s\n", filename);
    printf("content_type: %s\n", content_type);
    printf("transfer_encoding: %s\n", transfer_encoding);
    printf("data: %s\n", data);
    printf("off: %lu\n", off);
    printf("size: %lu\n", size);

    if (0 == strcmp (key, "username") &&
            (0 == strcmp (data, user)))
    {
        printf("Success username!!!\n");
        con_info->checkUsername = 1;
    }
    else {
        printf("NOT Success username!!!\n");
    }

    if ((0 == strcmp (key, "password")) &&
            (0 == strcmp (data, pass)))
    {
        printf("Success password!!!\n");
        con_info->checkPass = 1;
    }
    else {
        printf("NOT Success password!!!\n");
    }

    if(con_info->checkPass && con_info->checkUsername)
    {
        int len = strlen(path_content) + MAXNAMESIZE;
        char filePath[len];
        strcpy(filePath, path_content);
        printf("\n\n\nPath dir: %s\n", filePath);

        strcat(filePath, "/top_file.html");
        printf("Path file: %s\n", filePath);

        con_info->pathfile = filePath;
        printf("Success pathFile: %s\n", con_info->pathfile);
    }

    coninfo_cls = (void *)con_info;

    return MHD_YES;
}

static void request_completed (void *cls,
                               struct MHD_Connection *connection,
                               void **con_cls,
                               enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct *con_info = static_cast<connection_info_struct*>(*con_cls);

    if (NULL == con_info)
        return;

    if (con_info->connectiontype == POST)
        MHD_destroy_post_processor (con_info->postprocessor);

    free (con_info);
    *con_cls = NULL;
}

static MHD_Result answer_to_connection (void *cls,
                                        struct MHD_Connection *connection,
                                        const char *url,
                                        const char *method,
                                        const char *version,
                                        const char *upload_data,
                                        size_t *upload_data_size,
                                        void **con_cls)
{
    printf("\n\nConnect!\n");
    printf("URL: %s\n", url);
    printf("Method: %s\n", method);

    // инициализация парметров
    if (NULL == *con_cls)
    {
        struct connection_info_struct *con_info;

        con_info = static_cast<connection_info_struct*>(malloc(sizeof(struct connection_info_struct)));

        if (NULL == con_info)
            return MHD_NO;

        printf("Init con_cls\n");

        con_info->pathfile = NULL;
        con_info->checkPass = 0;
        con_info->checkUsername = 0;

        if (0 == strcmp (method, "POST"))
        {
            printf("Init postprocessor");
            con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void *) con_info);

            if (NULL == con_info->postprocessor)
            {
                free (con_info);
                return MHD_NO;
            }

            con_info->connectiontype = POST;
            con_info->ptr_connection = connection;
        }
        else
            con_info->connectiontype = GET;

        *con_cls = (void *)con_info;

        return MHD_YES;
    }

    if (0 == strcmp (method, "GET"))
        return answer_to_get_request(connection, url);

    if (0 == strcmp (method, "POST"))
    {
        struct connection_info_struct *con_info = static_cast<connection_info_struct*>(*con_cls);
        return answer_to_post_request(connection, con_info, url, upload_data, upload_data_size);
    }

    return sendPage(connection, errorServerPage);


    return MHD_NO;
}

int main(int argc, char *argv[])
{
    struct MHD_Daemon *daemon = nullptr;

    daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                               &answer_to_connection, NULL,
                               MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                               NULL, MHD_OPTION_END);
    printf("Start!\n");

    if (NULL == daemon)
    {
        printf("Daemon not started\n");
        return 1;
    }

    getchar ();

    MHD_stop_daemon (daemon);
    return 0;
}
