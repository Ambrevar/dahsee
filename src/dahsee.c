/* -*- mode: C -*- */
/*******************************************************************************
 * @file dahsee.c
 * @date 2012-06-28
 * @brief Dbus monitoring tool
 ******************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <dbus/dbus.h>

// Web interface with microhttpd.
// Needs string.h, stdio.h, stdlib.h
#include <microhttpd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#define PORT 7117

// XML support
#define XML_DELIMS_NODES "<>"
#define XML_DELIMS_PROPERTIES " "
#define XML_DELIMS_VALUES "\"="

#define XML_NODE_OBJECT "node"
#define XML_NODE_SIGNAL "signal"
#define XML_NODE_METHOD "method"
#define XML_NODE_INTERFACE "interface"
#define XML_NODE_ARG "arg"

#define XML_PROPERTY_NAME "name"
#define XML_PROPERTY_TYPE "type" // ARG only
#define XML_PROPERTY_DIRECTION "direction" // ARG only

#define XML_VALUE_DIRECTION_IN "in"
#define XML_VALUE_DIRECTION_OUT "out"

struct xml_dict
{
    char key;
    char* value;
};

struct xml_dict xml_value_type_table[] = {
    { 'y', "Byte"         },
    { 'b', "Boolean"      },
    { 'n', "Int16"        },
    { 'q', "Uint16"       },
    { 'i', "Int32"        },
    { 'u', "Uint32"       },
    { 'x', "Int64"        },
    { 't', "Uint64"       },
    { 'd', "Double"       },
    { 's', "String"       },
    { 'o', "Object Path"  },
    { 'g', "Signature"    },
    { 'a', "Array of"     },
    { '(', "Struct ("     },
    { ')', ")"            },
    { 'v', "Variant"      },
    { '{', "Dict entry {" },
    { '}', "}"            },
    { 'h', "Unix fd"      },
    { '\0', NULL          }
};

// Reserved. Not used for now according to D-Bus specification.
// Should be checked for future update.
/* static xml_value_type_table = { */
/*     { 'm', "Reserved 1" }, */
/*     { '*', "Reserved 2" }, */
/*     { '?', "Reserved 3" }, */
/*     { '^', "Reserved 41" }, */
/*     { '&', "Reserved 42" }, */
/*     { '@', "Reserved 43" }, */
/*     { '\0', NULL } */
/* } */


// JSON
// This format is used as internal storage, as well as export.
// Let's use an embedded implementation here.
#include "json.h"


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// TODO: temp vars. Clean later.
#define SPY_BUS "spy.lair"
#define FILTER_SIZE 1024
#define TEMPFILE "temp.json"

/**
 * Timestamps.
 * time_begin serves as chrono reference.
 * time_end serves as chrono stop.
 * How to use the results:
 *
 *   printf ("%lu.", time_end.tv_sec - time_begin.tv_sec);
 *   printf ("%.6lu\n", time_end.tv_usec - time_begin.tv_usec);
 *
 */
static struct timeval time_begin;
static struct timeval time_end;

// TODO: remove later
static inline void timer_print()
{
    printf ("%lu.", time_end.tv_sec - time_begin.tv_sec);
    printf ("%.6lu\n", time_end.tv_usec - time_begin.tv_usec);
}


/**
 * Web page
 */
static int
answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
    // TODO: add to logfile.
    printf ("URL=[%s]\n", url);
    /* printf ("METHOD=[%s]\n", method); */
    /* printf ("VERSION=[%s]\n", version); */
    /* printf ("DATA=[%s]\n", upload_data); */
    /* printf ("DATA SIZE=[%lu]\n", *upload_data_size); */
    
    FILE *fp;
    char *file_path;

    if (0== strcmp(url,"/"))
    {
        file_path = "index.html";
    }
    else
    {
        file_path = malloc(strlen(url)+2 );
        strcpy(file_path, ".");
        strcat(file_path ,url);        
    }

    fp = fopen(file_path, "r") ;

    size_t read_amount;
    char *page;
    if (fp==NULL)
    {
        perror(file_path);
        page  = "<html><body>Page not found!</body></html>";
        read_amount=strlen(page);
        /* return MHD_NO; */
    }
    else
    {
        fseek(fp, 0, SEEK_END);
        unsigned long fp_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        page=malloc( fp_len * sizeof (char) );
        
        read_amount = fread( page, sizeof(char), fp_len, fp);
        /* printf ("CHECK =[%lu] ", res ); */

        fclose(fp);
        
    }

    struct MHD_Response *response;
    int ret;

    /* printf ("SIZE=[%lu]\n",strlen(page) ); */
    response =
        MHD_create_response_from_buffer ( read_amount, (void *) page,
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

/**
 * List names on "session bus".
 */
void list()
{
    // Send method call.
    // ListNames on                  

    DBusMessage* msg;
    DBusMessageIter args;
    DBusMessageIter name_list;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    char* name;
    printf("List on %s, %s, %s\n",
           DBUS_SERVICE_DBUS,
           DBUS_PATH_DBUS,
           DBUS_INTERFACE_DBUS
        );

    // Init error
    dbus_error_init(&err);
        
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set(&err))
    {
        fprintf (stderr, "Connection Error (%s)", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL)
    {
        exit(1);
    }


    // create a new method call and check for errors
    msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, // target for the method call
                                       DBUS_PATH_DBUS, // object to call on
                                       DBUS_INTERFACE_DBUS, // interface to call on
                                       "ListNames"); // method name

    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    if (NULL == pending) { 
        fprintf(stderr, "Pending Call Null\n"); 
        exit(1); 
    }
    dbus_connection_flush(conn);
   
    printf("Request Sent\n");
   
    // free message
    dbus_message_unref(msg);
   
    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n"); 
        exit(1); 
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&args))
        fprintf(stderr, "Argument is not an array!\n");
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_element_type(&args))
    {
        fprintf(stderr,"Element type received: %d\n",
                dbus_message_iter_get_element_type(&args));
        fprintf(stderr,"Expected : %d\n", DBUS_TYPE_STRING);
    }

    dbus_message_iter_recurse(&args, &name_list);

    while (dbus_message_iter_next(&name_list))
    {
        // Should check for type, but assume OK since session bus.
        dbus_message_iter_get_basic(&name_list, &name);
        printf("Bus name: %s\n", name);
    }
   
    // free reply and close connection
    dbus_message_unref(msg);   
}

/**
 * List activable names on "session bus".
 */
void listAct()
{
    // Send method call.
    // ListNames on                  

    DBusMessage* msg;
    DBusMessageIter args;
    DBusMessageIter name_list;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    char* name;
    printf("List on %s, %s, %s\n",
           DBUS_SERVICE_DBUS,
           DBUS_PATH_DBUS,
           DBUS_INTERFACE_DBUS
        );

    // Init error
    dbus_error_init(&err);
        
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set(&err))
    {
        fprintf (stderr, "Connection Error (%s)", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL)
    {
        exit(1);
    }


    // create a new method call and check for errors
    msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, // target for the method call
                                       DBUS_PATH_DBUS, // object to call on
                                       DBUS_INTERFACE_DBUS, // interface to call on
                                       "ListActivatableNames"); // method name

    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    if (NULL == pending) { 
        fprintf(stderr, "Pending Call Null\n"); 
        exit(1); 
    }
    dbus_connection_flush(conn);
   
    printf("Request Sent\n");
   
    // free message
    dbus_message_unref(msg);
   
    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n"); 
        exit(1); 
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&args))
    {
        fprintf(stderr, "Argument is not an array!\n");
        dbus_message_iter_get_basic(&args, &name);
        printf("Error msg: %s\n", name);
        return;
    }
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_element_type(&args))
    {
        fprintf(stderr,"Element type received: %d\n",
                dbus_message_iter_get_element_type(&args));
        fprintf(stderr,"Expected : %d\n", DBUS_TYPE_STRING);
    }

    dbus_message_iter_recurse(&args, &name_list);

    while (dbus_message_iter_next(&name_list))
    {
        // Should check for type, but assume OK since session bus.
        dbus_message_iter_get_basic(&name_list, &name);
        printf("Bus name: %s\n", name);
    }
   
    // free reply and close connection
    dbus_message_unref(msg);   
}


/**
 * Get Name Owner
 */
void getNameOwner()
{
    // Send method call.
    // ListNames on                  

    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    char* name;

    // Init error
    dbus_error_init(&err);
        
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set(&err))
    {
        fprintf (stderr, "Connection Error (%s)", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL)
    {
        exit(1);
    }


    // create a new method call and check for errors
    msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, // target for the method call
                                       DBUS_PATH_DBUS, // object to call on
                                       DBUS_INTERFACE_DBUS, // interface to call on
                                       "GetNameOwner"); // method name

    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    char* param="org.naquadah.awesome.awful";

    // append arguments
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param)) {
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    if (NULL == pending) { 
        fprintf(stderr, "Pending Call Null\n"); 
        exit(1); 
    }
    dbus_connection_flush(conn);
   
    printf("Request Sent\n");
   
    // free message
    dbus_message_unref(msg);
   
    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n"); 
        exit(1); 
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
    {
        fprintf(stderr, "Argument is not a string!\n");
        return;
    }

    // Should check for type, but assume OK since session bus.
    dbus_message_iter_get_basic(&args, &name);
    printf("Name owner: %s\n", name);
    
   
    // free reply and close connection
    dbus_message_unref(msg);   
}

/**
 * Get Connexion Unix User
 */
void getConnectionUnixUser()
{
    // Send method call.
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    uint32_t name;

    // Init error
    dbus_error_init(&err);
        
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set(&err))
    {
        fprintf (stderr, "Connection Error (%s)", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL)
    {
        exit(1);
    }


    // create a new method call and check for errors
    msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, // target for the method call
                                       DBUS_PATH_DBUS, // object to call on
                                       DBUS_INTERFACE_DBUS, // interface to call on
                                       "GetConnectionUnixUser"); // method name

    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    char* param="org.naquadah.awesome.awful";

    // append arguments
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param)) {
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    if (NULL == pending) { 
        fprintf(stderr, "Pending Call Null\n"); 
        exit(1); 
    }
    dbus_connection_flush(conn);
   
    printf("Request Sent\n");
   
    // free message
    dbus_message_unref(msg);
   
    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n"); 
        exit(1); 
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args))
    {
        fprintf(stderr, "Argument is not a Uint32!\n");
        return;
    }

    // Should check for type, but assume OK since session bus.
    dbus_message_iter_get_basic(&args, &name);
    printf("Name owner: %lu\n", name);
    
   
    // free reply and close connection
    dbus_message_unref(msg);   
}


/**
 * Get Connexion Unix Process ID
 */
void getConnectionUnixProcessID()
{
    // Send method call.
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    uint32_t name;

    // Init error
    dbus_error_init(&err);
        
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set(&err))
    {
        fprintf (stderr, "Connection Error (%s)", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL)
    {
        exit(1);
    }


    // create a new method call and check for errors
    msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, // target for the method call
                                       DBUS_PATH_DBUS, // object to call on
                                       DBUS_INTERFACE_DBUS, // interface to call on
                                       "GetConnectionUnixProcessID"); // method name

    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    char* param="org.naquadah.awesome.awful";

    // append arguments
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param)) {
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    if (NULL == pending) { 
        fprintf(stderr, "Pending Call Null\n"); 
        exit(1); 
    }
    dbus_connection_flush(conn);
   
    printf("Request Sent\n");
   
    // free message
    dbus_message_unref(msg);
   
    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n"); 
        exit(1); 
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args))
    {
        fprintf(stderr, "Argument is not a Uint32!\n");
        return;
    }

    // Should check for type, but assume OK since session bus.
    dbus_message_iter_get_basic(&args, &name);
    printf("Process ID: %lu\n", name);
    
   
    // free reply and close connection
    dbus_message_unref(msg);   
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
    struct timeval time_machine;
    struct tm * time_human;
    char* type;
    unsigned int flag;

    if (gettimeofday (&time_machine, NULL) < 0)
    {
        perror("TIME");
        return;
    }

    // Use "localtime" or "gmtime" to get date and time from the elapsed seconds
    // since epoch.
    /* time_human = localtime( &(time_machine.tv_sec) ); */
    time_human = gmtime( &(time_machine.tv_sec) );

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
    json_append_member(export_object, "type", export_type);

    struct JsonNode *export_time = json_mkobject();
    struct JsonNode *export_time_sec = json_mknumber(time_machine.tv_sec);
    struct JsonNode *export_time_usec = json_mknumber(time_machine.tv_usec);
    json_append_member(export_time, "sec", export_time_sec);
    json_append_member(export_time, "usec", export_time_usec);
    json_append_member(export_object, "time", export_time);

    struct JsonNode *export_time_human = json_mkobject();
    struct JsonNode *export_time_human_hour = json_mknumber(time_human->tm_hour);
    struct JsonNode *export_time_human_min = json_mknumber(time_human->tm_min);
    struct JsonNode *export_time_human_sec = json_mknumber(time_human->tm_sec);
    json_append_member(export_time_human, "hour", export_time_human_hour);
    json_append_member(export_time_human, "min", export_time_human_min);
    json_append_member(export_time_human, "sec", export_time_human_sec);
    json_append_member(export_object, "time (human)", export_time_human);

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
    /* char* sigvalue; */

    char filter[FILTER_SIZE];

    /* printf("Listening for signals\n"); */

    // Initialise the errors API.
    dbus_error_init(&err);
   
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set ( &err ) ) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL) {
        exit(1);
    }

   
    // Register monitor on bus.
    dbus_bus_request_name(conn, SPY_BUS, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    // Message filters.
    snprintf(filter, FILTER_SIZE, "%s","type='signal'");
    snprintf(filter, FILTER_SIZE, "%s","");

    dbus_bus_add_match(conn, filter, &err); // see signals from the given interface
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Match Error (%s)\n", err.message);
        exit(1);
    }

    /* printf("Match rule sent.\n"); */
    /* printf("### Filter: %s\n",filter); */

    // TEMP: arbitrary limit.
    int i;
    for ( i=0 ; i<50 ; i++ )
    {
        // non blocking read of the next available message
        dbus_connection_read_write_dispatch(conn, -1);
        msg = dbus_connection_pop_message(conn);

        if(msg != NULL)
        {

            message_mangler (msg);

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

// TODO: support escaped character for delimiters.
// Nodes can be
// 1. Object
// 2. Interfaces
// 3. Members (Method or Signal)
// 4. Args

// Warning: this parser is not exhaustive, this will only work for D-Bus XML
// Introspection as follows:

/**
 * <!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
 * "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
 * <node>
 *   <interface name="org.freedesktop.DBus.Introspectable">
 *     <method name="Introspect">
 *       <arg name="data" direction="out" type="s"/>
 *     </method>
 *   </interface>
 * </node>
 */
void xmlparser(char* source)
{
    char* token;

    char* property; // Name, Type, Direction 
    char* value; // Property value

    char *saveptr1, *saveptr2, *saveptr3;

    // Skip header on first iteration.
    strtok_r(source, XML_DELIMS_NODES, &saveptr1);

    // Node slice
    for ( ; ; )
    {
        token = strtok_r(NULL, XML_DELIMS_NODES, &saveptr1);

        if (token == NULL)
            break;

        // Skip space between XML nodes.
        // Warning: this works if there is ONLY one node per line, and '\n' right after it.
        if (token[0] == '\n')
            continue;

        // Skip XML closure.
        if (token[0] == '/')
            continue;

        /* puts("=============================================================================="); */

        // Node Type
        token = strtok_r(token, XML_DELIMS_PROPERTIES, &saveptr2);
        if (token == NULL) // Happens if node has spaces only.
            continue;

        if (strcmp(token, XML_NODE_OBJECT) == 0)
            puts("Create object");
        else if (strcmp(token, XML_NODE_INTERFACE) == 0)
            puts("Create interface");
        else if (strcmp(token, XML_NODE_METHOD) == 0)
            puts("Create method");
        else if (strcmp(token, XML_NODE_SIGNAL) == 0)
            puts("Create signal");
        else if (strcmp(token, XML_NODE_ARG) == 0)
            puts("Create arg");
        else // Should never happen if input is as expected.
        {
            printf("ERROR2: could not recognize token (%s)\n",token);
            break;
        }

        // Properties
        for ( ; ; )
        {
            token = strtok_r(NULL, XML_DELIMS_PROPERTIES, &saveptr2);

            if (token == NULL)
                break;

            property = strtok_r(token, XML_DELIMS_VALUES, &saveptr3);
            if (property == NULL) // Shoud never happen.
                break;
            // Skip closing '/'.
            if (property[0] == '/')
                continue;

            value = strtok_r(NULL, XML_DELIMS_VALUES, &saveptr3);
            if (value == NULL)
                continue;

            if (strcmp(property, XML_PROPERTY_NAME) == 0)
                printf ("\tName: %s\n", value);
            else if (strcmp(property, XML_PROPERTY_DIRECTION) == 0)
                printf ("\tDirection: %s\n", value);
            else if (strcmp(property, XML_PROPERTY_TYPE) == 0)
            {
                printf ("\tType: ");

                int len=strlen(value);
                int i,j;

                for (i = 0; i < len; i++)
                {
                    for (j=0; xml_value_type_table[j].key != '\0'; j++)
                    {
                        if (xml_value_type_table[j].key == value[i])
                        {
                            printf ("%s ", xml_value_type_table[j].value );
                            break;
                        }
                    }
                    if (xml_value_type_table[j].key == '\0')
                    {
                        puts("");
                        printf ("==> ERROR: Type char (%c) not known.", value[i] );
                        break;    
                    }
                    
                }
                puts("");
            }
            // Should never happen.
            else
            {
                printf("ERROR3: could not recognize token (%s)\n",token);
                break;
            }
        }
    }
}

void introspect()
{
    // Send method call.
    DBusMessage* msg;
    DBusMessageIter args;
    /* DBusMessageIter name_list; */
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    char* name;

    // Init error
    dbus_error_init(&err);
        
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set(&err))
    {
        fprintf (stderr, "Connection Error (%s)", err.message);
        dbus_error_free(&err);
    }
    if (conn == NULL)
    {
        exit(1);
    }

    // create a new method call and check for errors
    msg = dbus_message_new_method_call(
        DBUS_SERVICE_DBUS, // target for the method call object to call on.
        /* "org.naquadah.awesome.awful", */
        // TODO: Useless?????
        DBUS_PATH_DBUS,
        /* DBUS_INTERFACE_DBUS, // interface to call on */
        "org.freedesktop.DBus.Introspectable", // interface to call on
        "Introspect"); // method name

    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    if (NULL == pending) { 
        fprintf(stderr, "Pending Call Null\n"); 
        exit(1); 
    }
    dbus_connection_flush(conn);
   
    printf("Request Sent\n");
   
    // free message
    dbus_message_unref(msg);
   
    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n"); 
        exit(1); 
    }

    // free the pending message handle
    dbus_pending_call_unref(pending);
    
    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
        fprintf(stderr, "Argument is not a string!\n");

    dbus_message_iter_get_basic(&args, &name);

    printf("Intro: %s\n", name);
    xmlparser(name);

    // free reply and close connection
    dbus_message_unref(msg);   
    
}


static void
run_daemon()
{
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                               &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == daemon)
        return;

    // TODO: handle signal.
    for ( ;  ;  )
    {
        sleep(60);
    }

    MHD_stop_daemon (daemon);

    return;
}

static inline void
print_help()
{
    printf ("Syntax: %s [-s] [-d] [<param>]\n\n", APPNAME);

    puts("Usage:");
    puts("  -d : Daemonize.");
    puts("  -h : Print this help.");
    puts("  -s : Spy all signals.");
    puts("  -v : Print version.");
}

static inline void
print_version()
{
    printf ("%s %s\n", APPNAME, VERSION);
    printf ("Copyright © %s %s\n", YEAR, AUTHOR);
    /* printf ("MIT License\n"); */
    /* printf ("This is free software: you are free to change and redistribute it.\n"); */
    /* printf ("There is NO WARRANTY, to the extent permitted by law.\n"); */
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
            print_help();
            return 0;
        case 's':
            run_spy=true;
            break;
        case 'v':
            print_version();
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

        // TODO: need to log somewhere, otherwise will go to terminal.
        run_daemon();
    }


    // Functions.
    if (run_spy == true)
    {
        spy();
    }

    /* jsonexport(); */

    return 0;
}
