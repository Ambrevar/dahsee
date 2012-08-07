/* -*- mode: C -*- */
/*******************************************************************************
 * @file dahsee.c
 * @date 2012-06-28
 * @brief Dbus monitoring tool
 ******************************************************************************/

#include <signal.h>
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

static volatile sig_atomic_t doneflag = 0;

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
#define XML_PROPERTY_TYPE "type"        // ARG only
#define XML_PROPERTY_DIRECTION "direction"      // ARG only

#define XML_VALUE_DIRECTION_IN "in"
#define XML_VALUE_DIRECTION_OUT "out"

struct xml_dict
{
    char key;
    char *value;
};

struct xml_dict dbus_value_type_table[] = {
    {'y', "Byte"},
    {'b', "Boolean"},
    {'n', "Int16"},
    {'q', "Uint16"},
    {'i', "Int32"},
    {'u', "Uint32"},
    {'x', "Int64"},
    {'t', "Uint64"},
    {'d', "Double"},
    {'s', "String"},
    {'o', "Object Path"},
    {'g', "Signature"},
    {'a', "Array of"},
    {'(', "Struct ("},
    {')', ")"},
    {'v', "Variant"},
    {'{', "Dict entry {"},
    {'}', "}"},
    {'h', "Unix fd"},
    {'\0', NULL}
};

// Reserved. Not used for now according to D-Bus specification.
// Should be checked for future update.
/* static dbus_value_type_table_extra = { */
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
#define JSON_FORMAT "    "

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
#ifdef __APPLE__
#    define TIME_FORMAT "%s\t%lu\t%d"
#else
#    define TIME_FORMAT "%s\t%lu\t%lu"
#endif

static struct timeval time_begin;
static struct timeval time_end;

// TODO: remove later
static inline void
timer_print ()
{
    printf ("%lu.", time_end.tv_sec - time_begin.tv_sec);
    printf ("%.6lu\n", time_end.tv_usec - time_begin.tv_usec);
}



/*******************************************************************************
 ******************************************************************************/


/**
 * JSON
 */
/* static void jsonimport() */
/* { */
/*     /\* struct json_object *new_obj; *\/ */

/*     char *input; */
/*     FILE* tempfile; */
/*     unsigned long pos; */

/*     tempfile  = fopen(TEMPFILE,"rb"); */
/*     if (access(TEMPFILE, R_OK) != 0) */
/*     { */
/*         perror(TEMPFILE); */
/*         return; */
/*     } */

/*     // File buffering. */
/*     fseek(tempfile, 0, SEEK_END); */
/*     pos = ftell(tempfile); */
/*     fseek(tempfile, 0, SEEK_SET); */

/*     input = malloc(pos*sizeof(char)); */
/*     fread(input, sizeof(char), pos, tempfile); */
/*     fclose(tempfile); */

/*     // Parsing. */
/*     /\* new_obj = json_tokener_parse(input); *\/ */
/*     /\* printf("new_obj.to_string()=%s\n\n", json_object_to_json_string(new_obj)); *\/ */

/*     /\* json_object_put(new_obj); *\/ */

/*     return; */
/* } */

/**
 * Profile mode (one line print).
 *
 * TODO: implement function.
 *
 *  printf (TIME_FORMAT, type, t.tv_sec, t.tv_usec);
 *
 *  if (flag & FLAG_SERIAL)
 *      printf ("\t%u", dbus_message_get_serial (message));
 *
 *  if (flag & FLAG_REPLY_SERIAL)
 *      printf ("\t%u", dbus_message_get_reply_serial (message));
 *
 *  if (flag & FLAG_SENDER)
 *      printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_sender (message)));
 *
 *  if (flag & FLAG_DESTINATION)
 *      printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_destination (message)));
 *
 *  if (flag & FLAG_PATH)
 *      printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_path (message)));
 *
 *  if (flag & FLAG_INTERFACE)
 *      printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_interface (message)));
 *
 *  if (flag & FLAG_MEMBER)
 *      printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_member (message)));
 *
 *  if (flag & FLAG_ERROR_NAME)
 *      printf ("\t%s", TRAP_NULL_STRING (dbus_message_get_error_name (message)));
 *
 *  printf ("\n");
 */


// TODO: temp vars. Clean later.
/* #define SPY_BUS "spy.lair" */
/* #define FILTER_SIZE 1024 */
#define TEMPFILE "temp.json"

void
JsonPrint(JsonNode *message, const char* path)
{
    if (access (path, W_OK) != 0)
    {
        perror (path);
        return;
    }
    // Binary mode is useless on POSIX.
    FILE* output = fopen (path, "a");

    char *tmp = json_stringify (message, JSON_FORMAT);
    fwrite (tmp, sizeof (char), strlen (tmp), output);
    fwrite ("\n", sizeof (char), 1, output);

    free (tmp);
    fclose (output);
}


/**
 * Bus Queries
 *
 * D-Bus provides a service with method that can return useful details about
 * current session / system. We use one function to centralize this: the
 * 'query' parameter will define what information we want.
 */
// TODO: stupid enum?
enum Queries
{
    QUERY_LIST_NAMES = 1,
    QUERY_ACTIVATABLE_NAMES = 2,
    QUERY_GET_NAME_OWNER = 4,
    QUERY_GET_CONNECTION_UNIX_USER = 8,
    QUERY_GET_CONNECTION_UNIX_PROCESS_ID = 16,
    QUERY_INTROSPECT = 32
};

void
queryBus(int query, char* parameter)
{
    DBusConnection *connection;
    DBusError error;

    DBusMessage *message = NULL;
    DBusMessageIter args;
    DBusMessageIter subargs;
    DBusBasicValue* argvalue;
    bool need_parameter = false;

    DBusPendingCall *pending;

    // Init
    dbus_error_init (&error);

    connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Connection Error (%s)", error.message);
        dbus_error_free (&error);
    }
    if (connection == NULL)
        exit (1);


    // Create a new method call.
    switch(query)
    {
    case QUERY_LIST_NAMES:
    {
        message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS,
                                                "ListNames");
        break;
    }

    case QUERY_ACTIVATABLE_NAMES:
    {
        message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS,
                                                "ListActivatableNames");
        break;
    }

    case QUERY_GET_NAME_OWNER:
    {
        message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS,
                                                "GetNameOwner");
        need_parameter = true;
        break;
    }

    case QUERY_GET_CONNECTION_UNIX_USER:
    {
        message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS,
                                                "GetConnectionUnixUser");
        need_parameter = true;
        break;
    }

    case QUERY_GET_CONNECTION_UNIX_PROCESS_ID:
    {
        message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS,
                                                "GetConnectionUnixProcessID");
        need_parameter = true;
        break;
    }

    case QUERY_INTROSPECT:
    {
        // TODO: Need to check if parameter is a valid busname.
        message = dbus_message_new_method_call (parameter,
                                                DBUS_PATH_DBUS,
                                                "org.freedesktop.DBus.Introspectable",
                                                "Introspect");
        break;
    }

    default:
        fprintf (stderr, "Query unrecognized.\n");
        return;
    }

    if (message == NULL)
    {
        fprintf (stderr, "Message creation error.\n");
        return;
    }


    if(need_parameter)
    {
        // Append parameter.
        dbus_message_iter_init_append (message, &args);
        if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &parameter))
        {
            fprintf (stderr, "Out Of Memory!\n");
            return;
        }
    }


    // Send message and get a handle for a reply.
    // -1 is default timeout.
    if (!dbus_connection_send_with_reply (connection, message, &pending, -1))
    {
        fprintf (stderr, "Out Of Memory!\n");
        return;
    }
    if (pending == NULL)
    {
        fprintf (stderr, "Pending Call Null\n");
        return;
    }

    // Force sending request without waiting.
    dbus_connection_flush (connection);

    // Free message.
    dbus_message_unref (message);

    // Block until we recieve a reply.
    dbus_pending_call_block (pending);

    // Get the reply message.
    message = dbus_pending_call_steal_reply (pending);
    if (message == NULL)
    {
        fprintf (stderr, "Reply Null\n");
        return;
    }

    // Free the pending message handle.
    dbus_pending_call_unref (pending);

    // Read the arguments.
    if (!dbus_message_iter_init (message, &args))
    {
        fprintf (stderr, "Message has no arguments!\n");
        return;
    }
    else
    {
        // TODO: is it useful to report errors here?
        switch(query)
        {
        case QUERY_LIST_NAMES:
        case QUERY_ACTIVATABLE_NAMES:
        {
            if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type (&args))
            {
                /* fprintf (stderr, "Argument is not an array!\n"); */
                return;
            }
            
            /* printf ("Bus names:\n"); */
            dbus_message_iter_recurse (&args, &subargs);

            while (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type (&subargs))
            {
                dbus_message_iter_get_basic (&subargs, &argvalue);
                printf ("%s\n", (char*) argvalue);
                dbus_message_iter_next (&subargs);
            }

            break;
        }

        case QUERY_GET_NAME_OWNER:
        case QUERY_INTROSPECT:
        {
            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type (&args))
            {
                dbus_message_iter_get_basic (&args, &argvalue);
                printf ("%s\n", (char*) argvalue);
            }
            break;
        }

        case QUERY_GET_CONNECTION_UNIX_USER:
        case QUERY_GET_CONNECTION_UNIX_PROCESS_ID:
        {
            if (DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type (&args))
            {
                dbus_message_iter_get_basic (&args, &argvalue);
                printf ("%lu\n", (dbus_uint32_t) argvalue);
            }
            break;
        }

        }
    }

    // free reply and close connection
    dbus_message_unref (message);
    dbus_connection_unref (connection);
}



/**
 * Members attributes.
 *
 * TODO: to be completed.
 *
 * TODO: why can signals have destination? D-Bus reference says they are always
 * broadcasted.
 *
 * All messages have:
 * -type
 * -time
 * -sender (optional)
 * -destination (optional)
 * -arguments i.e. data payload (optional)
 * 
 * Errors have:
 * -error name
 * -reply serial
 *
 * Method calls have:
 * -serial
 * -object path
 * -interface (optional)
 * -member name
 *
 * Method returns have:
 * -reply_serial
 *
 * Signals have:
 * -serial
 * -object path
 * -interface
 * -member name
 *
 * Arguments can be of the following types:
 *
 * DBUS_TYPE_ARRAY
 * DBUS_TYPE_BOOLEAN
 * DBUS_TYPE_BYTE
 * DBUS_TYPE_DICT_ENTRY
 * DBUS_TYPE_DOUBLE
 * DBUS_TYPE_INT16
 * DBUS_TYPE_INT32
 * DBUS_TYPE_INT64
 * DBUS_TYPE_INVALID
 * DBUS_TYPE_OBJECT_PATH
 * DBUS_TYPE_SIGNATURE
 * DBUS_TYPE_STRING
 * DBUS_TYPE_STRUCT
 * DBUS_TYPE_UINT16
 * DBUS_TYPE_UINT32
 * DBUS_TYPE_UINT64
 * DBUS_TYPE_UNIX_FD
 * DBUS_TYPE_VARIANT
 *
 * DBUS_STRUCT_BEGIN_CHAR_AS_STRING
 * DBUS_STRUCT_END_CHAR_AS_STRING
 * DBUS_TYPE_ARRAY_AS_STRING
 * DBUS_TYPE_BOOLEAN_AS_STRING
 * DBUS_TYPE_BYTE_AS_STRING
 * DBUS_TYPE_DICT_ENTRY_AS_STRING
 * DBUS_TYPE_DOUBLE_AS_STRING
 * DBUS_TYPE_INT16_AS_STRING
 * DBUS_TYPE_INT32_AS_STRING
 * DBUS_TYPE_INT64_AS_STRING
 * DBUS_TYPE_INVALID_AS_STRING
 * DBUS_TYPE_OBJECT_PATH_AS_STRING
 * DBUS_TYPE_SIGNATURE_AS_STRING
 * DBUS_TYPE_STRING_AS_STRING
 * DBUS_TYPE_STRUCT_AS_STRING
 * DBUS_TYPE_UINT16_AS_STRING
 * DBUS_TYPE_UINT32_AS_STRING
 * DBUS_TYPE_UINT64_AS_STRING
 * DBUS_TYPE_UNIX_FD_AS_STRING
 * DBUS_TYPE_VARIANT_AS_STRING
 */

// TODO: stolen from dbus-print-message.c. Rewrite?
static void
print_iter (DBusMessageIter * iter)
{
    do
    {
        int type = dbus_message_iter_get_arg_type (iter);

        if (type == DBUS_TYPE_INVALID)
            break;

        switch (type)
        {
            case DBUS_TYPE_STRING:
                {
                    char *val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("string \"");
                    printf ("%s", val);
                    printf ("\"\n");
                    break;
                }

            case DBUS_TYPE_SIGNATURE:
                {
                    char *val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("signature \"");
                    printf ("%s", val);
                    printf ("\"\n");
                    break;
                }

            case DBUS_TYPE_OBJECT_PATH:
                {
                    char *val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("object path \"");
                    printf ("%s", val);
                    printf ("\"\n");
                    break;
                }

            case DBUS_TYPE_INT16:
                {
                    dbus_int16_t val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("int16 %d\n", val);
                    break;
                }

            case DBUS_TYPE_UINT16:
                {
                    dbus_uint16_t val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("uint16 %u\n", val);
                    break;
                }

            case DBUS_TYPE_INT32:
                {
                    dbus_int32_t val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("int32 %d\n", val);
                    break;
                }

            case DBUS_TYPE_UINT32:
                {
                    dbus_uint32_t val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("uint32 %u\n", val);
                    break;
                }

            case DBUS_TYPE_INT64:
                {
                    dbus_int64_t val;
                    dbus_message_iter_get_basic (iter, &val);
#ifdef DBUS_INT64_PRINTF_MODIFIER
                    printf ("int64 %" DBUS_INT64_PRINTF_MODIFIER "d\n", val);
#else
                    printf ("int64 (omitted)\n");
#endif
                    break;
                }

            case DBUS_TYPE_UINT64:
                {
                    dbus_uint64_t val;
                    dbus_message_iter_get_basic (iter, &val);
#ifdef DBUS_INT64_PRINTF_MODIFIER
                    printf ("uint64 %" DBUS_INT64_PRINTF_MODIFIER "u\n", val);
#else
                    printf ("uint64 (omitted)\n");
#endif
                    break;
                }

            case DBUS_TYPE_DOUBLE:
                {
                    double val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("double %g\n", val);
                    break;
                }

            case DBUS_TYPE_BYTE:
                {
                    unsigned char val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("byte %d\n", val);
                    break;
                }

            case DBUS_TYPE_BOOLEAN:
                {
                    dbus_bool_t val;
                    dbus_message_iter_get_basic (iter, &val);
                    printf ("boolean %s\n", val ? "true" : "false");
                    break;
                }

            case DBUS_TYPE_VARIANT:
                {
                    DBusMessageIter subiter;

                    dbus_message_iter_recurse (iter, &subiter);

                    printf ("variant ");
                    print_iter (&subiter);
                    break;
                }
            case DBUS_TYPE_ARRAY:
                {
                    // TODO: needs some testing.

                    int array_type;
                    DBusMessageIter subiter;

                    int array_len;
                    DBusBasicValue *array;

                    dbus_message_iter_recurse (iter, &subiter);
                    array_type = dbus_message_iter_get_arg_type (&subiter);

                    if (dbus_type_is_fixed (array_type) == FALSE)
                    {
                        // Not fixed.
                        printf ("[");
                        while (array_type != DBUS_TYPE_INVALID)
                        {
                            print_iter (&subiter);

                            dbus_message_iter_next (&subiter);
                            array_type =
                                dbus_message_iter_get_arg_type (&subiter);

                            if (array_type != DBUS_TYPE_INVALID)
                                printf (",");
                        }
                        printf ("]\n");
                    }
                    else
                    {
                        dbus_message_iter_get_fixed_array (&subiter, &array,
                                                           &array_len);
                        // FIXME: there may be an issue here with the unknown type.
                        int i;
                        printf ("[");
                        for (i = 0; i < array_len - 1; i++)
                        {
                            printf ("%lu,", array[i]);
                        }
                        printf ("%lu", array[array_len - 1]);
                        printf ("]\n");
                    }

                    break;
                }
            case DBUS_TYPE_DICT_ENTRY:
                {
                    DBusMessageIter subiter;

                    dbus_message_iter_recurse (iter, &subiter);

                    printf ("dict entry(\n");
                    print_iter (&subiter);
                    dbus_message_iter_next (&subiter);
                    print_iter (&subiter);
                    printf (")\n");
                    break;
                }

            case DBUS_TYPE_STRUCT:
                {
                    int current_type;
                    DBusMessageIter subiter;

                    dbus_message_iter_recurse (iter, &subiter);

                    printf ("struct {\n");
                    while ((current_type =
                            dbus_message_iter_get_arg_type (&subiter)) !=
                           DBUS_TYPE_INVALID)
                    {
                        print_iter (&subiter);
                        dbus_message_iter_next (&subiter);
                        if (dbus_message_iter_get_arg_type (&subiter) !=
                            DBUS_TYPE_INVALID)
                            printf (",");
                    }
                    printf ("}\n");
                    break;
                }

            case DBUS_TYPE_UNIX_FD:
                {
                    // TODO: print something?
                    printf ("Unix FD.\n");
                    break;
                }

            case DBUS_TYPE_INVALID:
                {
                    printf ("Invalid argument.\n");
                    break;
                }

            default:
                printf ("Warning: (%c) out of specification!\n", type);
                break;
        }
    }
    while (dbus_message_iter_next (iter));
}

/**
 * Create JSON object from message and return it. The created node should be
 * freed with json_delete(node);
 */
#define TRAP_NULL_STRING(str) ((str) ? (str) : "<none>")

enum Flags
{
    FLAG_SERIAL = 1,
    FLAG_REPLY_SERIAL = 2,
    FLAG_PATH = 4,
    FLAG_INTERFACE = 8,
    FLAG_MEMBER = 32,
    FLAG_ERROR_NAME = 64
};

// TODO: will probably need to add "defines" for all fields afterwards.
#define DBUS_JSON_TYPE "type"

static struct JsonNode*
message_mangler (DBusMessage * message)
{
    struct JsonNode *message_node = json_mkobject ();
    unsigned int flag = 0;

    // TODO: add index? (message 1, message 2, etc.)

    // TIME
    struct timeval time_machine;
    struct tm *time_human;
    if (gettimeofday (&time_machine, NULL) < 0)
    {
        perror ("Could not get time.");
        return NULL;
    }

    // TODO: use argv parameter to request human readable time and local/UTC.
    // Use "localtime" or "gmtime" to get date and time from the elapsed seconds
    // since epoch.
    /* time_human = localtime( &(time_machine.tv_sec) ); */
    time_human = gmtime (&(time_machine.tv_sec));

    json_append_member (message_node, "sec", json_mknumber (time_machine.tv_sec));
    json_append_member (message_node, "usec", json_mknumber (time_machine.tv_usec));

    // TODO: make this field optional.
    struct JsonNode *time_node = json_mkobject ();
    json_append_member (time_node, "hour", json_mknumber (time_human->tm_hour) );
    json_append_member (time_node, "min", json_mknumber (time_human->tm_min) );
    json_append_member (time_node, "sec", json_mknumber (time_human->tm_sec) );
    json_append_member (message_node, "time_human", time_node);

    // TYPE
    switch (dbus_message_get_type (message))
    {
        // TODO: check if serial is different / needed for error and method return.
        case DBUS_MESSAGE_TYPE_ERROR:
            json_append_member (message_node, DBUS_JSON_TYPE, json_mkstring("error") );
            flag = FLAG_SERIAL | FLAG_ERROR_NAME | FLAG_REPLY_SERIAL;
            break;

        case DBUS_MESSAGE_TYPE_METHOD_CALL:
            json_append_member (message_node, DBUS_JSON_TYPE, json_mkstring("method_call"));
            flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER;
            break;

        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
            json_append_member (message_node, DBUS_JSON_TYPE, json_mkstring("method_return"));
            flag = FLAG_SERIAL | FLAG_REPLY_SERIAL;
            break;

        case DBUS_MESSAGE_TYPE_SIGNAL:
            json_append_member (message_node, DBUS_JSON_TYPE, json_mkstring("signal"));
            flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER;
            break;

        default:
            json_append_member (message_node, DBUS_JSON_TYPE, json_mkstring("unknown"));
            flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER | FLAG_ERROR_NAME | FLAG_REPLY_SERIAL;
            break;
    }

    // SENDER
    json_append_member (message_node, "sender",
                        json_mkstring ( TRAP_NULL_STRING ( dbus_message_get_sender (message) )));

    // DESTINATION
    json_append_member (message_node, "destination",
                        json_mkstring ( TRAP_NULL_STRING ( dbus_message_get_destination (message) )));

    // FLAGS
    if (flag & FLAG_SERIAL)
        json_append_member (message_node, "serial",
                            json_mknumber (  dbus_message_get_serial (message) ));

    if (flag & FLAG_REPLY_SERIAL)
        json_append_member (message_node, "reply_serial",
                            json_mknumber (  dbus_message_get_reply_serial (message) ));

    // TRAP_NULL_STRING is not needed here if specification is correctly handled.
    if (flag & FLAG_PATH)
    json_append_member (message_node, "path",
                        json_mkstring ( TRAP_NULL_STRING ( dbus_message_get_path (message) )));

    // Interface is optional for method calls.
    if (flag & FLAG_INTERFACE)
    json_append_member (message_node, "interface",
                        json_mkstring ( TRAP_NULL_STRING ( dbus_message_get_interface (message) )));

    if (flag & FLAG_MEMBER)
    json_append_member (message_node, "member",
                        json_mkstring ( TRAP_NULL_STRING ( dbus_message_get_member (message) )));

    // TRAP_NULL_STRING is not needed here if specification is correctly handled.
    if (flag & FLAG_ERROR_NAME)
    json_append_member (message_node, "error_name",
                        json_mkstring ( TRAP_NULL_STRING ( dbus_message_get_error_name (message) )));


    return message_node;
}

/**
 * Eavesdrop messages and store them internally.  Parameter is a user-defined
 * filter. Filter syntax is as defined by D-Bus specification. Example:
 *
 *   "type='signal',sender='org.gnome.TypingMonitor',interface='org.gnome.TypingMonitor'"
 *
 * If filter=="all", then all messages are handled.
 */

#define EAVES "eavesdrop=true"

void
spy(char* filter)
{
    DBusConnection *connection;
    DBusError error;

    DBusMessage *message = NULL;
    DBusMessageIter args;


    // Should never happen;
    if (filter == NULL)
    {
        return;
    }

    if (strcmp(filter, "all")==0)
    {
        filter = "";
    }

    /**
     * Message filters.
     * Type are among:
     * -signal
     * -method_call
     * -method_return
     * -error
     */
    // Note: eavesdropping role is being prepended so that is can be turned off
    // by user in the filter.
    size_t filterlength = strlen(EAVES) + 1 + strlen(filter) + 1;
    char* eavesfilter = malloc( filterlength * sizeof(char) );
    snprintf( eavesfilter, filterlength, "%s,%s", EAVES, filter);

    // Init
    dbus_error_init (&error);

    connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Connection Error (%s)", error.message);
        dbus_error_free (&error);
    }
    if (connection == NULL)
        exit (1);

    dbus_bus_add_match (connection, eavesfilter, &error);
    // TODO: useless here?
    dbus_connection_flush (connection);
    if (dbus_error_is_set (&error))
    {
        fprintf (stderr, "Match Error (%s)\n", error.message);
        exit (1);
    }

    // TODO: remove this line later.
    printf ("### Filter: %s\n", eavesfilter);

    for (;;)
    {
        // Blocking read (-1) of the next available message.
        dbus_connection_read_write_dispatch (connection, -1);
        message = dbus_connection_pop_message (connection);

        if (message != NULL)
        {
            JsonNode* message_node = message_mangler (message);
            JsonPrint(message_node, TEMPFILE);
            json_delete(message_node);

            // TODO: needs testing.
            // Read the arguments.
            /* if (dbus_message_iter_init (message, &args)) */
            /*     print_iter (&args); */

            /* // free the message */
            dbus_message_unref (message);
        }
    }

    dbus_connection_unref (connection);
}

/**
 * XML Parser
 *
 * TODO: support escaped character for delimiters.
 *
 * Nodes can be
 * 1. Object
 * 2. Interfaces
 * 3. Members (Method or Signal)
 * 4. Args
 *
 * Warning: this parser is not exhaustive, this will only work for D-Bus XML
 * Introspection as follows:
 *
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
void
dbus_xml_parser (char *source)
{
    char *token;

    char *property;             // Name, Type, Direction 
    char *value;                // Property value

    char *saveptr1, *saveptr2, *saveptr3;

    // Skip header on first iteration.
    strtok_r (source, XML_DELIMS_NODES, &saveptr1);

    // Node slice
    for (;;)
    {
        token = strtok_r (NULL, XML_DELIMS_NODES, &saveptr1);

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
        token = strtok_r (token, XML_DELIMS_PROPERTIES, &saveptr2);
        if (token == NULL)      // Happens if node has spaces only.
            continue;

        if (strcmp (token, XML_NODE_OBJECT) == 0)
            puts ("Create object");
        else if (strcmp (token, XML_NODE_INTERFACE) == 0)
            puts ("Create interface");
        else if (strcmp (token, XML_NODE_METHOD) == 0)
            puts ("Create method");
        else if (strcmp (token, XML_NODE_SIGNAL) == 0)
            puts ("Create signal");
        else if (strcmp (token, XML_NODE_ARG) == 0)
            puts ("Create arg");
        else                    // Should never happen if input is as expected.
        {
            printf ("ERROR2: could not recognize token (%s)\n", token);
            break;
        }

        // Properties
        for (;;)
        {
            token = strtok_r (NULL, XML_DELIMS_PROPERTIES, &saveptr2);

            if (token == NULL)
                break;

            property = strtok_r (token, XML_DELIMS_VALUES, &saveptr3);
            if (property == NULL)       // Shoud never happen.
                break;
            // Skip closing '/'.
            if (property[0] == '/')
                continue;

            value = strtok_r (NULL, XML_DELIMS_VALUES, &saveptr3);
            if (value == NULL)
                continue;

            if (strcmp (property, XML_PROPERTY_NAME) == 0)
                printf ("\tName: %s\n", value);
            else if (strcmp (property, XML_PROPERTY_DIRECTION) == 0)
                printf ("\tDirection: %s\n", value);
            else if (strcmp (property, XML_PROPERTY_TYPE) == 0)
            {
                printf ("\tType: ");

                int len = strlen (value);
                int i, j;

                for (i = 0; i < len; i++)
                {
                    for (j = 0; dbus_value_type_table[j].key != '\0'; j++)
                    {
                        if (dbus_value_type_table[j].key == value[i])
                        {
                            printf ("%s ", dbus_value_type_table[j].value);
                            break;
                        }
                    }
                    if (dbus_value_type_table[j].key == '\0')
                    {
                        puts ("");
                        printf ("==> ERROR: Type char (%c) not known.",
                                value[i]);
                        break;
                    }

                }
                puts ("");
            }
            // Should never happen.
            else
            {
                printf ("ERROR3: could not recognize token (%s)\n", token);
                break;
            }
        }
    }
}

/**
 * Web page
 */
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
        file_path = "index.html";
    }
    else
    {
        file_path = malloc (strlen (url) + 2);
        strcpy (file_path, ".");
        strcat (file_path, url);
    }

    fp = fopen (file_path, "r");

    size_t read_amount;
    char *page;
    if (fp == NULL)
    {
        perror (file_path);
        page = "<html><body>Page not found!</body></html>";
        read_amount = strlen (page);
        /* return MHD_NO; */
    }
    else
    {
        fseek (fp, 0, SEEK_END);
        unsigned long fp_len = ftell (fp);
        fseek (fp, 0, SEEK_SET);

        page = malloc (fp_len * sizeof (char));

        read_amount = fread (page, sizeof (char), fp_len, fp);
        /* printf ("CHECK =[%lu] ", res ); */

        fclose (fp);

    }

    struct MHD_Response *response;
    int ret;

    /* printf ("SIZE=[%lu]\n",strlen(page) ); */
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


static void
run_daemon ()
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

static void
print_help ()
{
    printf ("Syntax: %s [-a|-d|-h|-i NAME|-l|-n NAME|-p NAME|-s FILTER|-u NAME|-v]\n\n", APPNAME);

    puts ("Usage:");
    puts ("  -a: List activatable bus names.");
    puts ("  -d: Daemonize.");
    puts ("  -h: Print this help.");
    puts ("  -i: Returns introspection of NAME.");
    puts ("  -l: List registered bus names.");
    puts ("  -n: Returns unique name associated to NAME.");
    puts ("  -p: Returns PID associated to NAME.");
    puts ("  -s: Spy signals. FILTER syntax follows D-Bus specification. If FILTER=='all', all messages are caught.");
    puts ("  -u: Returns UID who owns NAME.");
    puts ("  -v: Print version.");
}

static void
print_version ()
{
    printf ("%s %s\n", APPNAME, VERSION);
    printf ("Copyright © %s %s\n", YEAR, AUTHOR);
    /* printf ("MIT License\n"); */
    /* printf ("This is free software: you are free to change and redistribute it.\n"); */
    /* printf ("There is NO WARRANTY, to the extent permitted by law.\n"); */
}


void
nice_exit (int sig)
{
    printf ("Print this before closing\n");
    exit (sig);
}

int
main (int argc, char **argv)
{
    // Fork variables.
    bool daemonize = false;
    pid_t pid, sid;

    // getopt() variables.
    int c;
    extern char *optarg;
    extern int optind;
    extern int optopt;


    // Catch Ctrl-C.
    struct sigaction act;
    act.sa_handler = nice_exit;
    act.sa_flags = 0;

    if ((sigemptyset (&act.sa_mask) == -1) ||
        (sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        return 1;
    }

    while ((c = getopt (argc, argv, ":adhi:ln:p:s:vu:")) != -1)
    {
        switch (c)
        {
        case 'a':
            queryBus (QUERY_ACTIVATABLE_NAMES, NULL);
            return 0;
        case 'd':
            daemonize = true;
            break;
        case 'h':
            print_help ();
            return 0;
        case 'i':
            queryBus (QUERY_INTROSPECT, optarg);
            return 0;
        case 'l':
            queryBus (QUERY_LIST_NAMES, NULL);
            return 0;
        case 'n':
            queryBus (QUERY_GET_NAME_OWNER, optarg);
            return 0;
        case 'p':
            queryBus (QUERY_GET_CONNECTION_UNIX_PROCESS_ID, optarg);
            return 0;
        case 's':
            spy(optarg);
            break;
        case 'v':
            print_version ();
            return 0;
        case 'u':
            queryBus (QUERY_GET_CONNECTION_UNIX_USER, optarg);
            return 0;
        case ':':
            printf ("-%c needs an argument.\n", optopt);
            break;
        case '?':
            printf ("Unknown argument %c.\n", optopt);
            break;
        default:
            print_help();
            return 0;
        }
    }


    if (daemonize == true)
    {
        /* Fork off the parent process */
        pid = fork ();
        if (pid < 0)
        {
            exit (EXIT_FAILURE);
        }
        if (pid > 0)
        {
            exit (EXIT_SUCCESS);
        }

        // Change the file mode mask following parameters.
        /* umask(0);        */

        /* Create a new SID for the child process */
        sid = setsid ();
        if (sid < 0)
        {
            /* Log any failure here */
            exit (EXIT_FAILURE);
        }

        /* Change the current working directory */
        /* if ((chdir("/")) < 0) { */
        /*     /\* Log any failure here *\/ */
        /*     exit(EXIT_FAILURE); */
        /* }     */

        // TODO: need to log somewhere, otherwise will go to terminal.
        run_daemon ();
    }

    return 0;
}
