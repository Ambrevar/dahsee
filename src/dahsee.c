/* -*- mode: C -*- */
/*******************************************************************************
 * @file dahsee.c
 * @date 2012-06-28
 * @brief Dbus monitoring tool
 ******************************************************************************/

#include <dbus/dbus.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// Data storage.
// JSON: Let's use a static json lib here.
#include "json.h"
/* #include "ccan/json/json.h" */
/* #include <json/json.h> */


// TODO: temp vars. Clean later.
#define SPY_BUS "spy.lair"
#define FILTER_SIZE 1024
#define TEMPFILE "temp.json"

/**
 * Timestamps.
 * time_begin serves as chrono reference.
 * time_end serves as chrono stop.
 * timer_print() is a developper function to show how to use the results.
 */
static struct timeval time_begin;
static struct timeval time_end;

static inline void timer_print()
{
    printf ("%lu.", time_end.tv_sec - time_begin.tv_sec);
    printf ("%.6lu\n", time_end.tv_usec - time_begin.tv_usec);
}


/**
 * JSON
 */
static void jsonimport()
{
    /* struct json_object *new_obj; */

    char *input;
    FILE* tempfile;
    unsigned long pos;

    tempfile  = fopen(TEMPFILE,"rb");
    if (access(TEMPFILE, R_OK) != 0)
    {
        perror(TEMPFILE);
        return;
    }

    // File buffering.
    fseek(tempfile, 0, SEEK_END);
    pos = ftell(tempfile);
    fseek(tempfile, 0, SEEK_SET);

    input = malloc(pos*sizeof(char));
    fread(input, sizeof(char), pos, tempfile);
    fclose(tempfile);

    // Parsing.
    /* new_obj = json_tokener_parse(input); */
    /* printf("new_obj.to_string()=%s\n\n", json_object_to_json_string(new_obj)); */

    /* json_object_put(new_obj); */

    return;
}

static void jsonexport()
{
    struct JsonNode *export_object;
    struct JsonNode *testbool = json_mkbool(true);
    struct JsonNode *testnum = json_mknumber(3.1415);
    struct JsonNode *teststr = json_mkstring("€€€€ €€€€");

    FILE* export_file;

    // Binary mode is useless on POSIX.
    export_file  = fopen(TEMPFILE,"w");
    if (access(TEMPFILE, W_OK) != 0)
    {
        perror(TEMPFILE);
        return;
    }

    export_object = json_mkobject();
    json_append_member(export_object, "testbool", testbool);
    json_append_member(export_object, "teststr", teststr);
    json_append_member(export_object, "testnum", testnum);

    char * tmp=json_stringify(export_object,"    ");

    // Note: this works with UTF-8.
    /* fwrite( tmp , sizeof(char), strlen(tmp), export_file  ); */

    free(tmp);

    json_delete(export_object);

    fclose(export_file);
    return;
}

#ifdef __APPLE__
#define TIME_FORMAT "%s\t%lu\t%d"
#else
#define TIME_FORMAT "%s\t%lu\t%lu"
#endif
#define TRAP_NULL_STRING(str) ((str) ? (str) : "<none>")

enum Flags
{
    FLAG_SERIAL = 1,
    FLAG_REPLY_SERIAL = 2,
    FLAG_SENDER = 4,
    FLAG_DESTINATION = 8,
    FLAG_PATH = 16,
    FLAG_INTERFACE = 32,
    FLAG_MEMBER = 64,
    FLAG_ERROR_NAME = 128
} ;

static void
message_mangler (DBusMessage *message)
{
    struct timeval t;
    char* type;
    unsigned int flag;

    if (gettimeofday (&t, NULL) < 0)
    {
        perror("TIME");
        return;
    }

    switch (dbus_message_get_type (message))
    {
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
        type = "Method Call";
        flag = FLAG_SERIAL |
            FLAG_SENDER |
            FLAG_PATH |
            FLAG_INTERFACE |
            FLAG_MEMBER;
        break;

    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        type = "Method return";
        flag= FLAG_SERIAL |
            FLAG_DESTINATION |
            FLAG_REPLY_SERIAL;
        break;

    case DBUS_MESSAGE_TYPE_ERROR:
        type = "Error";
        flag = FLAG_SERIAL |
            FLAG_DESTINATION |
            FLAG_REPLY_SERIAL;
        break;

    case DBUS_MESSAGE_TYPE_SIGNAL:
        type="Signal";
        flag=FLAG_SERIAL |
            FLAG_PATH |
            FLAG_INTERFACE |
            FLAG_MEMBER;
        break;

    default:
        printf ("Unknown message.\n");
        break;
    }

    // Print
    /* printf (TIME_FORMAT, type, t.tv_sec, t.tv_usec); */

    /* if (flag & FLAG_SERIAL) */
    /*     printf ("\t%u", dbus_message_get_serial (message)); */

    /* if (flag & FLAG_REPLY_SERIAL) */
    /*     printf ("\t%u", dbus_message_get_reply_serial (message)); */

    /* if (flag & FLAG_SENDER) */
    /*     printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_sender (message))); */

    /* if (flag & FLAG_DESTINATION) */
    /*     printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_destination (message))); */

    /* if (flag & FLAG_PATH) */
    /*     printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_path (message))); */

    /* if (flag & FLAG_INTERFACE) */
    /*     printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_interface (message))); */

    /* if (flag & FLAG_MEMBER) */
    /*     printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_member (message))); */

    /* if (flag & FLAG_ERROR_NAME) */
    /*     printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_error_name (message))); */

    /* printf ("\n"); */

    // JSON Export
    struct JsonNode *export_object;
    export_object = json_mkobject();

    struct JsonNode *export_type = json_mkstring(type);
    struct JsonNode *export_time = json_mkobject();
    struct JsonNode *export_time_sec = json_mknumber(t.tv_sec);
    struct JsonNode *export_time_usec = json_mknumber(t.tv_usec);

    json_append_member(export_object, "type", export_type);
    json_append_member(export_time, "time_sec", export_time_sec);
    json_append_member(export_time, "time_usec", export_time_usec);
    json_append_member(export_object, "time", export_time);

    if (flag & FLAG_SERIAL)
    {
        struct JsonNode *export_serial = json_mknumber(dbus_message_get_serial (message));
        json_append_member(export_object, "serial", export_serial);
    }

    if (flag & FLAG_REPLY_SERIAL)
    {
        struct JsonNode *export_reply_serial = json_mknumber(dbus_message_get_reply_serial (message));
        json_append_member(export_object, "reply_serial", export_reply_serial);
    }

    if (flag & FLAG_SENDER)
    {
        struct JsonNode *export_sender = json_mkstring(TRAP_NULL_STRING (dbus_message_get_sender (message)));
        json_append_member(export_object, "sender", export_sender);
    }

    if (flag & FLAG_DESTINATION)
    {
        struct JsonNode *export_destination = json_mkstring(TRAP_NULL_STRING (dbus_message_get_destination (message)));
        json_append_member(export_object, "destination", export_destination);
    }

    if (flag & FLAG_PATH)
    {
        struct JsonNode *export_path = json_mkstring(TRAP_NULL_STRING (dbus_message_get_path (message)));
        json_append_member(export_object, "path", export_path);
    }

    if (flag & FLAG_INTERFACE)
    {
        struct JsonNode *export_interface = json_mkstring(TRAP_NULL_STRING (dbus_message_get_interface (message)));
        json_append_member(export_object, "interface", export_interface);
   }

    if (flag & FLAG_MEMBER)
    {
        struct JsonNode *export_member = json_mkstring(TRAP_NULL_STRING (dbus_message_get_member (message)));
        json_append_member(export_object, "member", export_member);
    }

    if (flag & FLAG_ERROR_NAME)
    {
        struct JsonNode *export_error_name = json_mkstring(TRAP_NULL_STRING (dbus_message_get_error_name (message)));
        json_append_member(export_object, "error_name", export_error_name);
    }

    FILE* export_file;

    // Binary mode is useless on POSIX.
    export_file  = fopen(TEMPFILE,"a");
    if (access(TEMPFILE, W_OK) != 0)
    {
        perror(TEMPFILE);
        return;
    }


    char * tmp=json_stringify(export_object,"    ");
    fwrite(tmp, sizeof(char), strlen(tmp), export_file);
    fwrite("\n", sizeof(char), 1, export_file);

    // Clean
    free(tmp);
    json_delete(export_object);
    fclose(export_file);
}

/**
 * Catch all signals.
 */
static void spy(/* char* param */)
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    char* sigvalue;

    char filter[FILTER_SIZE];

    printf("Listening for signals\n");

    // initialise the errors
    dbus_error_init(&err);
   
    /* // connect to the bus and check for errors */
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set ( &err ) ) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (NULL == conn) {
        exit(1);
    }

   
    // request our name on the bus and check for errors
    dbus_bus_request_name(conn, SPY_BUS, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    // add a rule for which messages we want to see
    /* snprintf(filter, FILTER_SIZE, "%s","type='signal'"); */
    snprintf(filter, FILTER_SIZE, "%s","");

    dbus_bus_add_match(conn, filter, &err); // see signals from the given interface
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Match Error (%s)\n", err.message);
        exit(1);
    }


    printf("Match rule sent.\n");
    printf("### Filter: %s\n",filter);

    int i;
    for ( i=0 ; i<100 ; i++ )
    {
        // non blocking read of the next available message
        dbus_connection_read_write_dispatch(conn, -1);
        msg = dbus_connection_pop_message(conn);

        if(msg != NULL)
        {

            /* message_mangler (msg); */

            /* // check if the message is a signal from the correct interface and with the correct name */
            /* /\* if (dbus_message_is_signal(msg, TEST_SIGNAL_INTERFACE, TEST_SIGNAL_NAME)) *\/ */
            /* /\* { *\/ */
            /* // read the parameters */
            /* if (!dbus_message_iter_init(msg, &args)) */
            /*     fprintf(stderr, "Message has no parameters\n"); */
            /* else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) */
            /*     fprintf(stderr, "Argument is not a string!\n"); */
            /* else */
            /* { */
            /*     dbus_message_iter_get_basic(&args, &sigvalue); */
         
            /*     printf("Got Signal value %s\n", sigvalue); */
            /* } */
            /* /\* } *\/ */
      
            /* // free the message */
            dbus_message_unref(msg);
        }
    }

    dbus_connection_unref(conn);
}

static inline void
print_help(const char* name)
{
    printf ("Syntax: %s [-s] [-d] [<param>]\n\n", name);

    puts("Usage:");
    puts("  -d : Daemonize.");
    puts("  -h : Print this help.");
    puts("  -s : Spy all signals.");
    puts("  -v : Print version.");
}

static inline void
print_version(const char* name)
{
    printf ("%s %s\n", name, VERSION);
}


int
main(int argc, char** argv)
{
    // Fork variables.
    bool daemonize=false;
    bool run_spy=false;
    pid_t pid=1, sid;

    // getopt() variables.
    int c;
    extern char* optarg;
    extern int optind;
    extern int optopt;

    while ( (c=getopt(argc,argv,":dhsv")) != -1)
    {
        switch(c)
        {
        case 'd':
            daemonize=true;
                break;
        case 'h':
            print_help(argv[0]);
            return 0;
        case 's':
            run_spy=true;
            break;
        case 'v':
            print_version(argv[0]);
            return 0;
        case ':':
            printf ("-%c needs an argument.\n", optopt);
            break;
        case '?':
            printf ("Unknown argument %c.\n", optopt);
            break;
        }
    }


    if (daemonize == true)
    {
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        
        // Change the file mode mask following parameters.
        /* umask(0);        */
        
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log any failure here */
            exit(EXIT_FAILURE);
        }
        
        /* Change the current working directory */
        /* if ((chdir("/")) < 0) { */
        /*     /\* Log any failure here *\/ */
        /*     exit(EXIT_FAILURE); */
        /* }     */

        // TODO: implement daemon.
        for (;;)
        {
            sleep(60);
        }
    }


    // Functions.
    if (run_spy == true)
    {
        spy();
    }

    /* jsonexport(); */

    return 0;
}
