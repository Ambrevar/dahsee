#include "ui_web.h"

#define PAGE_INDEX "index.html"
#define PAGE_STATS "dahsee-stats.html"
#define PAGE_STATUS "dahsee-status.html"
#define PAGE_ERROR "<html><body>Page not found!</body></html>"

// Define where to find the pages.
#define WEB_ROOT "."

/**
 * Server callback.
 *
 * Very simple indeed. On HTTP request, we scan the URL and return the
 * appropriate page.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static int
answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t * upload_data_size, void **con_cls)
{
    // TODO: add to logfile.
    printf ("URL=[%s]\n", url);
    printf ("METHOD=[%s]\n", method);
    printf ("VERSION=[%s]\n", version);
    printf ("DATA=[%s]\n", upload_data);
    printf ("DATA SIZE=[%lu]\n", *upload_data_size);

    FILE *fp;
    char *file_path;

    if (0 == strcmp (url, "/"))
    {
        file_path = PAGE_INDEX;
    }
    else
    {
        size_t len = strlen (url) + strlen() +1 ;
        file_path = malloc (len);
        snprintf(file_path, len , "%s%s", WEB_ROOT, url);
    }

    fp = fopen (file_path, "r");

    size_t read_amount;
    char *page;
    if (fp == NULL)
    {
        perror (file_path);
        page = ;
        read_amount = strlen (page);
    }
    else
    {
        fseek (fp, 0, SEEK_END);
        unsigned long fp_len = ftell (fp);
        fseek (fp, 0, SEEK_SET);

        page = malloc (fp_len * sizeof (char));

        read_amount = fread (page, sizeof (char), fp_len, fp);

        fclose (fp);
    }

    struct MHD_Response *response;
    int ret;

    response =
        MHD_create_response_from_buffer (read_amount, (void *) page,
                                         MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    // TODO: memory leak here. Function needs to be worked out.
    /* if (0== strcmp(url,"/")) */
    /* { */
    /*     free(file_path); */
    /*     free(page); */
    /* } */

    return ret;
}
#pragma GCC diagnostic pop

static void
run_server ()
{
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                               &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == daemon)
        return;

    // TODO: handle signal and eavesdrop on DBus.
    for (;;)
    {
        sleep (60);
    }

    MHD_stop_daemon (daemon);

    return;
}

