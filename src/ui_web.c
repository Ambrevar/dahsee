/* -*- mode: C -*- */
/*******************************************************************************
 * ui_web.c
 * 2012-08-13
 *
 * Dahsee - a D-Bus monitoring tool
 *
 * Copyright © 2012 Pierre Neidhardt
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

#include "ui_web.h"

#define PAGE_INDEX "dahsee.html"
#define PAGE_STATS "dahsee-stats.html"
#define PAGE_STATUS "dahsee-status.html"
#define PAGE_ERROR "<html><body>Page not found!</body></html>"

// Define where to find the pages.
// Use env XDG_DATA_DIRS
#define WEB_ROOT "../data/"

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
    fprintf (stderr, "URL=[%s]\n", url);
    fprintf (stderr, "METHOD=[%s]\n", method);
    fprintf (stderr, "VERSION=[%s]\n", version);
    fprintf (stderr, "DATA=[%s]\n", upload_data);
    fprintf (stderr, "DATA SIZE=[%lu]\n", *upload_data_size);

    if (0 == strcmp (url, "/"))
        // No url specified.
        url = PAGE_INDEX;

    // Append specified url to web page location.
    char* file_path;
    size_t len = strlen(WEB_ROOT) + strlen (url) + 1;
    file_path = malloc (len);
    snprintf(file_path, len , "%s%s", WEB_ROOT, url);

    FILE *fp;
    fp = fopen (file_path, "r");

    size_t read_amount;
    char *page;

    printf ("%s\n", file_path);

    if (fp == NULL)
    {
        // Web page could not be found.
        perror (file_path);
        page = PAGE_ERROR;
        read_amount = strlen (page);
    }
    else
    {
        fseek (fp, 0, SEEK_END);
        long fp_len = ftell (fp);
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

void
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

