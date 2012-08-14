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

#include <signal.h>
#include <stdbool.h>
#include "ui_web.h"

#define PAGE_INDEX "dahsee.html"
#define PAGE_MESSAGES "/dahsee-messages.html"
#define PAGE_MESSAGES_START "/sig/start"
#define PAGE_MESSAGES_STOP "/sig/stop"
#define PAGE_MESSAGES_END "dahsee-messages-end.html"
#define PAGE_STATISTICS "dahsee-statistics.html"
#define PAGE_STATUS "dahsee-status.html"
#define PAGE_END "\n</body>\n</html>\n"
#define PAGE_ERROR "Page not found!"
#define PAGE_ERROR_FULL "<html><body>Page not found!</body></html>"

// TODO: implement this.
// Define where to find the pages.
// Use env XDG_DATA_DIRS
#define WEB_ROOT "../data/"


// Points to index content. Set NULL by default, so that we can check if it has
// been already loaded, thus preventing multiple reloads.
static char* page_index = NULL;
static long page_index_len;

// TODO: dirty code!!!
#define DATA_ERROR "<td>Data error.</td>"
extern char* html_message;

// Engine prototypes we need here.
/* #include "json.h" */
/* #define LIVE_OUTPUT_OFF 0 */
/* #define LIVE_OUTPUT_ON 1 */
/* void spy (char *filter, int opt); */
/* char* html_message(JsonNode* message); */



static char* 
load_page(const char* url)
{
    // Append specified url to web page location.
    char* file_path;
    size_t len = strlen(WEB_ROOT) + strlen (url) + 1;
    file_path = malloc (len);
    snprintf(file_path, len , "%s%s", WEB_ROOT, url);

    FILE *fp;
    fp = fopen (file_path, "r");

    if (fp == NULL)
    {
        // Web page could not be found.
        perror (file_path);
        free(file_path);
        return NULL;
    }
    free(file_path);


    size_t page_len;
    char *page;

    // Page length.
    fseek (fp, 0, SEEK_END);
    page_len = ftell (fp) + 1;
    fseek (fp, 0, SEEK_SET);

    page = malloc( page_len * sizeof(char));

    fread (page, sizeof (char), page_len, fp);

    fclose(fp);

    return page;
}

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

    bool is_not_index_only = false;

    // If no URL specified.
    if (0 == strcmp (url, "/"))
        url = PAGE_INDEX;

    // If page_index is requested, we do not load the file another time.
    if (strcmp(url, PAGE_INDEX) != 0)
        is_not_index_only = true;

    /* if (0 == strcmp (url, PAGE_INDEX)) */
    /*     spy(NULL); */

    // Load page_index one time only.
    if (page_index == NULL)
    {
        page_index=load_page(PAGE_INDEX);
        page_index_len = strlen(page_index);
    }

    char* page ;
    size_t page_len ;

    // WARNING: do not load url on random assumptions!
    // Messages page.
    // FIXME: How to return nothing?
    if (strcmp(url, PAGE_MESSAGES_START) == 0)
    {
        kill(getpid(), SIGUSR1);
        page = PAGE_ERROR;
        page_len = strlen(page);
    }
    else if (strcmp(url, PAGE_MESSAGES_STOP) == 0)
    {
        kill(getpid(), SIGINT);
        page = PAGE_ERROR;
        page_len = strlen(page);
    }
    // Messages page.
    else if (strcmp(url, PAGE_MESSAGES) == 0)
    {
        char* subpage;
        subpage = load_page(PAGE_MESSAGES);
        if (subpage == NULL)
            subpage = PAGE_ERROR;

        char* subpage_end;
        subpage_end = load_page(PAGE_MESSAGES_END);
        if (subpage_end == NULL)
            subpage_end = PAGE_ERROR;

        char* content = html_message;
        if (content == NULL)
            content = DATA_ERROR;

        page_len = page_index_len
            + strlen(subpage)
            + strlen(content)
            + strlen(subpage_end)
            + strlen(PAGE_END)
            + 1;
        
        page = malloc ( page_len * sizeof(char));
    
        strcpy(page, page_index);
        strcat(page, subpage);
        strcat(page, content);
        strcat(page, subpage_end);
        strcat(page, PAGE_END);
    }
    // Other pages
    // TODO: check for security risks (like "..").
    else if (strstr(url, "html") != NULL)
    {
        char* subpage;
        if (is_not_index_only)
        {
            subpage = load_page(url);
            if (subpage == NULL)
                subpage = PAGE_ERROR;
            page_len = page_index_len + strlen(subpage) + strlen(PAGE_END) +1;
        }
        else
            page_len = page_index_len + strlen(PAGE_END) +1;

        page = malloc ( page_len * sizeof(char));
    
        strcpy(page, page_index);
        if (is_not_index_only)
            strcat(page, subpage);
        strcat(page, PAGE_END);
    }
    else
    {
        page = load_page( url);
        if (page == NULL)
            page = PAGE_ERROR;
        page_len = strlen(page);
    }

    struct MHD_Response *response;
    int ret;

    response =
        MHD_create_response_from_buffer (page_len, (void *) page,
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

// TODO: Need to set mutex, or simple test.
/* static void */
/* daemon_spy () */
/* { */
/*     fprintf (stderr, "%s\n", "BEGIN"); */
/*     spy(NULL, LIVE_OUTPUT_OFF); */
/*     extern JsonNode * message_array; */
/*     printf("%s\n", html_message(message_array)); */
/*     fprintf (stderr, "%s\n", "END"); */
/* } */

void
run_server ()
{
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                               &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == daemon)
        return;

    // Catch SIGUSR1
    /* struct sigaction act; */
    /* act.sa_handler = daemon_spy; */
    /* act.sa_flags = 0; */

    /* if ((sigemptyset (&act.sa_mask) == -1) || */
    /*     (sigaction (SIGUSR1, &act, NULL) == -1)) */
    /* { */
    /*     perror ("Failed to set SIGUSR1 handler"); */
    /*     return; */
    /* } */

    // TODO: handle signal and eavesdrop on DBus.
    for (;;)
    {
        sleep (60);
    }


    if (page_index != NULL)
        free(page_index);

    MHD_stop_daemon (daemon);

    return;
}

