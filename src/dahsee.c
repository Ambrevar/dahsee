/* -*- mode: C -*- */
/*******************************************************************************
 * @file dahsee.c
 * @date 2012-06-28
 * @brief D-Bus monitoring tool
 ******************************************************************************/

// TODO: check if all message_mangler calls get properly cleaned.

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

// Optional UI support
#if DAHSEE_UI_WEB != 0
#    include "ui_web.h"
#endif


/**
 * JSON
 *
 * This format is used as internal storage, as well as export.
 * Let's use an embedded implementation here.
 */
#include "json.h"

#define DBUS_JSON_TYPE "type"
#define DBUS_JSON_SEC "sec"
#define DBUS_JSON_USEC "usec"
#define DBUS_JSON_HOUR "hour"
#define DBUS_JSON_MINUTE "minute"
#define DBUS_JSON_SECOND "second"
#define DBUS_JSON_TIME_HUMAN "time_human"
#define DBUS_JSON_SENDER "sender"
#define DBUS_JSON_DESTINATION "destination"
#define DBUS_JSON_SERIAL "serial"
#define DBUS_JSON_REPLY_SERIAL "reply_serial"
#define DBUS_JSON_PATH "path"
#define DBUS_JSON_INTERFACE "interface"
#define DBUS_JSON_MEMBER "member"
#define DBUS_JSON_ERROR "error"
#define DBUS_JSON_METHOD_CALL "method_call"
#define DBUS_JSON_METHOD_RETURN "method_return"
#define DBUS_JSON_SIGNAL "signal"
#define DBUS_JSON_ERROR_NAME "error_name"
#define DBUS_JSON_ARGS "arg"
#define DBUS_JSON_UNKNOWN "unknown"

#define DBUS_JSON_ARG_TYPE "type"
#define DBUS_JSON_ARG_VALUE "value"
#define DBUS_JSON_ARG_ARRAYTYPE "arraytype"



/******************************************************************************/
/* Options                                                                    */
/******************************************************************************/
enum OutputFormat
{
    FORMAT_JSON,
    FORMAT_PROFILE,
    FORMAT_XML
};
static volatile sig_atomic_t doneflag = 0;
// TODO: check if output is alwaus properly closed on exit.
static FILE* output = NULL;
static FILE* input = NULL;
static const char *output_path = NULL;
static const char *input_path = NULL;
static bool option_force_overwrite = false;
static int option_output_format = FORMAT_JSON;
// Contains the whole bunch of messages caught using spy();
static JsonNode *message_array ;
#define JSON_FORMAT "  "
#define JSON_FORMAT_NONE ""


/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

static JsonNode*
json_import(const char * inputpath)
{
    char *inputstring;
    FILE* inputfile;
    long pos;

    inputfile  = fopen(inputpath,"r");
    if (inputpath==NULL)
    {
        perror("json_import");
    }

    // File buffering.
    fseek(inputfile, 0, SEEK_END);
    pos = ftell(inputfile);
    fseek(inputfile, 0, SEEK_SET);

    // WARNING: it is important not to forget the terminating null byte!
    inputstring = malloc((pos+1)*sizeof(char));
    fread(input, sizeof(char), pos, inputfile);
    fclose(inputfile);
    inputstring[pos]='\0';

    // Parsing.
    JsonNode* imported_json=NULL;
    imported_json = json_decode(inputstring);

    free(inputstring);
    return imported_json;
}


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

void
json_print (JsonNode * message)
{
    // Open output for the first time.
    if (output == NULL)
    {
        // To stdout by default.
        if (output_path == NULL)
            output = stdout;
        else
        {
            // Check if file exists, and if user allowed it to be overwritten.
            if (option_force_overwrite == false && access (output_path, F_OK) == 0)
            {
                perror (output_path);
                return;
            }

            // Note: binary mode is useless on POSIX.
            output = fopen (output_path, "w");
            if (output == NULL)
            {
                perror (output_path);
                return;
            }
        }
    }

    char *tmp = json_stringify (message, JSON_FORMAT);
    fwrite (tmp, sizeof (char), strlen (tmp), output);
    fwrite ("\n", sizeof (char), 1, output);

    free (tmp);
}

/**
 * XML support. Since we use JSON internally, XML is not convenient. So we
 * implement our own 150 lines XML-to-JSON parser.
 */
#define XML_DELIMS_NODES "<>"
#define XML_DELIMS_PROPERTIES " "
#define XML_DELIMS_VALUES "\"="

#define XML_NODE_OBJECT "node"
#define XML_NODE_SIGNAL "signal"
#define XML_NODE_METHOD "method"
#define XML_NODE_INTERFACE "interface"
#define XML_NODE_ARG "arg"

#define XML_PROPERTY_NAME "name"
#define XML_PROPERTY_TYPE "type"
#define XML_PROPERTY_DIRECTION "direction"

#define XML_VALUE_DIRECTION_IN "in"
#define XML_VALUE_DIRECTION_OUT "out"

struct xml_dict
{
    char key;
    char *value;
};

/**
 * Correspondance table for D-Bus type.
 *
 * The extra table is reserved. Not used for now according to D-Bus
 * specification.  Should be checked for future update of D-Bus specification.
 *
 * static dbus_value_type_table_extra = {
 *     { 'm', "Reserved 1" },
 *     { '*', "Reserved 2" },
 *     { '?', "Reserved 3" },
 *     { '^', "Reserved 41" },
 *     { '&', "Reserved 42" },
 *     { '@', "Reserved 43" },
 *     { '\0', NULL }
 * }
 */
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


/**
 * D-Bus XML Parser
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


// TODO: test without reentrency support.
struct JsonNode*
dbus_xml_parser (char * source)
{
    struct JsonNode* result = json_mkarray();
    struct JsonNode* current_node;

    // Node where current_node is being added.
    struct JsonNode* last_node_ptr;

    // The two variables are used to get access to the appropriate array.
    struct JsonNode* last_member_ptr = NULL;
    struct JsonNode* last_interface_ptr = NULL;

    char *token;

    // Name, Type, Direction 
    char *property;
    // Property value
    char *value;

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
        // WARNING: this works if there is ONLY one node per line, and '\n'
        // right after it.
        if (token[0] == '\n')
            continue;

        // Skip XML closure.
        if (token[0] == '/')
            continue;

        // Node Type
        token = strtok_r (token, XML_DELIMS_PROPERTIES, &saveptr2);

        // Should never happen (except if node has spaces only).
        if (token == NULL)
            continue;

        // Main switch. WARNING: this part is not safe at all if input is badly
        // formatted.
        if (strcmp (token, XML_NODE_OBJECT) == 0)
            last_node_ptr = result; 
        else if (strcmp (token, XML_NODE_INTERFACE) == 0)
        {
            last_node_ptr = result->children.tail;

            if (json_find_member(last_node_ptr, XML_NODE_INTERFACE)==NULL)
                json_append_member(last_node_ptr, XML_NODE_INTERFACE, json_mkarray());
            last_node_ptr=json_find_member(last_node_ptr, XML_NODE_INTERFACE);
            last_interface_ptr=last_node_ptr;
        }
        else if (strcmp (token, XML_NODE_METHOD) == 0)
        {
            last_node_ptr = last_interface_ptr->children.tail;

            if (json_find_member(last_node_ptr, XML_NODE_METHOD)==NULL)
                json_append_member(last_node_ptr, XML_NODE_METHOD, json_mkarray());
            last_node_ptr=json_find_member(last_node_ptr, XML_NODE_METHOD);
            last_member_ptr=last_node_ptr;
        }
        else if (strcmp (token, XML_NODE_SIGNAL) == 0)
        {
            last_node_ptr = last_interface_ptr->children.tail;

            if (json_find_member(last_node_ptr, XML_NODE_SIGNAL)==NULL)
                json_append_member(last_node_ptr, XML_NODE_SIGNAL, json_mkarray());
            last_node_ptr=json_find_member(last_node_ptr, XML_NODE_SIGNAL);
            last_member_ptr=last_node_ptr;
        }
        else if (strcmp (token, XML_NODE_ARG) == 0)
        {
            last_node_ptr = last_member_ptr->children.tail;

            if (json_find_member(last_node_ptr, XML_NODE_ARG)==NULL)
                json_append_member(last_node_ptr, XML_NODE_ARG, json_mkarray());
            last_node_ptr=json_find_member(last_node_ptr, XML_NODE_ARG);
        }

        // Should never happen if input is as expected.
        else
        {
            // TODO: lig this.
            printf ("ERROR: could not recognize token (%s)\n", token);
            break;
        }

        current_node = json_mkobject();

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
                json_append_member(current_node, XML_PROPERTY_NAME, json_mkstring(value));
            else if (strcmp (property, XML_PROPERTY_DIRECTION) == 0)
                json_append_member(current_node, XML_PROPERTY_DIRECTION, json_mkstring(value));
            else if (strcmp (property, XML_PROPERTY_TYPE) == 0)
            {
                size_t i,j;
                size_t arg_sym_size = strlen (value);
                char** array_ptr = malloc( arg_sym_size * sizeof (char*));
                char* arg_string;
                size_t arg_string_size = 0;

                for (i = 0; i < arg_sym_size; i++)
                {
                    for (j = 0; dbus_value_type_table[j].key != '\0'; j++)
                    {
                        if (dbus_value_type_table[j].key == value[i])
                        {
                            arg_string_size += strlen(dbus_value_type_table[j].value) + 1;
                            array_ptr[i]=dbus_value_type_table[j].value;
                            break;
                        }
                    }
                    
                    // Char not found in table.
                    if (dbus_value_type_table[j].key == '\0')
                    {
                        // TODO: log this.
                        printf ("ERROR: Type char (%c) not known.\n", value[i]);
                        break;
                    }
                }
                
                arg_string = calloc(arg_string_size , sizeof(char));
                strcat(arg_string, array_ptr[0]);
                for (i = 1; i < arg_sym_size; i++)
                {
                    strcat(arg_string, " ");
                    strcat(arg_string, array_ptr[i]);
                }

                json_append_member(current_node, XML_PROPERTY_TYPE, json_mkstring(arg_string));
            }
            // Should never happen.
            else
            {
                // TODO: log this.
                printf ("ERROR: could not recognize token (%s)\n", token);
                break;
            }

        } // End of properties.
        json_append_element(last_node_ptr, current_node);
    }

    return result;
}


/**
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
 */


/**
 * Since arguments in D-Bus can be complicated stuff, we dedicate a function to
 * transform the received arguments to a JSON structure.
 */
struct JsonNode *
args_mangler (DBusMessageIter * args)
{
    struct JsonNode *args_node = json_mkobject ();

    int type = dbus_message_iter_get_arg_type (args);

    // TODO: check if useful to log it or not. I guess so. See below.
    /* if (type == DBUS_TYPE_INVALID) */
    /*     break; */

    switch (type)
    {
    case DBUS_TYPE_STRING:
    {
        char *value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("string"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mkstring (value));
        break;
    }

    case DBUS_TYPE_SIGNATURE:
    {
        char *value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("signature"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mkstring (value));
        break;
    }

    // TODO: object_path or path?
    case DBUS_TYPE_OBJECT_PATH:
    {
        char *value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("object_path"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mkstring (value));
        break;
    }

    case DBUS_TYPE_INT16:
    {
        dbus_int16_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("int16"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    case DBUS_TYPE_UINT16:
    {
        dbus_uint16_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("uint16"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    case DBUS_TYPE_INT32:
    {
        dbus_int32_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("int32"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    case DBUS_TYPE_UINT32:
    {
        dbus_uint32_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("uint32"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    // TODO: check if 64 bit works properly.
    case DBUS_TYPE_INT64:
    {
        dbus_int64_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("int64"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    case DBUS_TYPE_UINT64:
    {
        dbus_uint64_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("uint64"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    case DBUS_TYPE_DOUBLE:
    {
        double value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("double"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    case DBUS_TYPE_BYTE:
    {
        unsigned char value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("byte"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    // TODO: bool or boolean?
    case DBUS_TYPE_BOOLEAN:
    {
        dbus_bool_t value;
        dbus_message_iter_get_basic (args, &value);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("bool"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            json_mknumber (value));
        break;
    }

    // TODO: Needs testing.
    case DBUS_TYPE_VARIANT:
    {
        DBusMessageIter subargs;
        dbus_message_iter_recurse (args, &subargs);
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("variant"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, 
                            args_mangler (&subargs));
        break;
    }

    // TODO: Needs testing. Seems to be ok.

    // Since D-Bus array elements are all of the same type, we store this
    // information along 'type' and 'value', 'value' being a JSON array with the
    // values. That's why we need to strip the type information from the
    // recursive call. But since JSON does not allow multiple parents, with must
    // remove the parent first.
    case DBUS_TYPE_ARRAY:
    {
        DBusMessageIter subargs;
        dbus_message_iter_recurse (args, &subargs);
        struct JsonNode *array = json_mkarray ();
        struct JsonNode *arraytype;
        struct JsonNode *arrayvalue;
        struct JsonNode *arrayarg;

        arraytype=json_find_member(args_mangler (&subargs), DBUS_JSON_ARG_TYPE);
        json_remove_from_parent(arraytype);

        while (dbus_message_iter_get_arg_type (&subargs) !=
               DBUS_TYPE_INVALID)
        {
            arrayarg=args_mangler (&subargs);
            arrayvalue=json_find_member(arrayarg, DBUS_JSON_ARG_VALUE);
            json_remove_from_parent(arrayvalue);
            json_append_element (array, arrayvalue);

            // TODO: CHECK if no JSON element is lost.
            json_delete(arrayarg);
            dbus_message_iter_next (&subargs);
        }

        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("array"));
        json_append_member (args_node, DBUS_JSON_ARG_ARRAYTYPE, arraytype);
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, array);

        break;
    }

    // TODO: check if JSON structure is right here.
    case DBUS_TYPE_DICT_ENTRY:
    {
        DBusMessageIter subargs;
        dbus_message_iter_recurse (args, &subargs);

        struct JsonNode *dict = json_mkarray ();
        json_append_element (dict, args_mangler (&subargs));
        dbus_message_iter_next (&subargs);
        json_append_element (dict, args_mangler (&subargs));

        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("dict_entry"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, dict);
        break;
    }

    case DBUS_TYPE_STRUCT:
    {
        DBusMessageIter subargs;
        dbus_message_iter_recurse (args, &subargs);
        struct JsonNode *structure = json_mkarray ();

        while (dbus_message_iter_get_arg_type (&subargs) !=
               DBUS_TYPE_INVALID)
        {
            json_append_element (structure, args_mangler (&subargs));
            dbus_message_iter_next (&subargs);
        }

        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("struct"));
        json_append_member (args_node, DBUS_JSON_ARG_VALUE, structure);
        break;
    }

    // TODO: write something else ?
    case DBUS_TYPE_UNIX_FD:
    {
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("unix_fd"));
        break;
    }

    case DBUS_TYPE_INVALID:
    {
        json_append_member (args_node, DBUS_JSON_ARG_TYPE, json_mkstring("invalid"));
        break;
    }

    // TODO: put this to log.
    default:
        printf ("Warning: (%c) out of specification!\n", type);
        break;
    }

    return args_node;
}


/**
 * Message mangler
 *
 * Creates JSON object from message and returns it.
 *
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

// WARNING: manual free with json_delete(node).
struct JsonNode *
message_mangler (DBusMessage * message)
{
    struct JsonNode *message_node = json_mkobject ();
    enum Flags flag = 0;

    // TIME
    struct timeval time_machine;
    struct tm *time_human;

    // TODO: check APPLE support since it is supposed to have a different struct
    // timeval.
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

    json_append_member (message_node, DBUS_JSON_SEC,
                        json_mknumber (time_machine.tv_sec));
    json_append_member (message_node, DBUS_JSON_USEC,
                        json_mknumber (time_machine.tv_usec));

    // TODO: make this field optional.
    struct JsonNode *time_node = json_mkobject ();
    json_append_member (time_node, DBUS_JSON_HOUR,
                        json_mknumber (time_human->tm_hour));
    json_append_member (time_node, DBUS_JSON_MINUTE, json_mknumber (time_human->tm_min));
    json_append_member (time_node, DBUS_JSON_SECOND, json_mknumber (time_human->tm_sec));
    json_append_member (message_node, DBUS_JSON_TIME_HUMAN, time_node);

    // TYPE
    switch (dbus_message_get_type (message))
    {
        // TODO: check if serial is different / needed for error and method return.
    case DBUS_MESSAGE_TYPE_ERROR:
        json_append_member (message_node, DBUS_JSON_TYPE,
                            json_mkstring (DBUS_JSON_ERROR));
        flag = FLAG_SERIAL | FLAG_ERROR_NAME | FLAG_REPLY_SERIAL;
        break;

    case DBUS_MESSAGE_TYPE_METHOD_CALL:
        json_append_member (message_node, DBUS_JSON_TYPE,
                            json_mkstring (DBUS_JSON_METHOD_CALL));
        flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER;
        break;

    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        json_append_member (message_node, DBUS_JSON_TYPE,
                            json_mkstring (DBUS_JSON_METHOD_RETURN));
        flag = FLAG_SERIAL | FLAG_REPLY_SERIAL;
        break;

    case DBUS_MESSAGE_TYPE_SIGNAL:
        json_append_member (message_node, DBUS_JSON_TYPE,
                            json_mkstring (DBUS_JSON_SIGNAL));
        flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER;
        break;

    default:
        json_append_member (message_node, DBUS_JSON_TYPE,
                            json_mkstring (DBUS_JSON_UNKNOWN));
        flag =
            FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER |
            FLAG_ERROR_NAME | FLAG_REPLY_SERIAL;
        break;
    }

    // TODO: get unique name for sender and destination instead of well-known
    // names. Well-knwwn names should be an option.

    // SENDER
    json_append_member (message_node, DBUS_JSON_SENDER,
                        json_mkstring (TRAP_NULL_STRING
                                       (dbus_message_get_sender (message))));

    // DESTINATION
    json_append_member (message_node, DBUS_JSON_DESTINATION,
                        json_mkstring (TRAP_NULL_STRING
                                       (dbus_message_get_destination
                                        (message))));

    // FLAGS
    if (flag & FLAG_SERIAL)
        json_append_member (message_node, DBUS_JSON_SERIAL,
                            json_mknumber (dbus_message_get_serial
                                           (message)));

    if (flag & FLAG_REPLY_SERIAL)
        json_append_member (message_node, DBUS_JSON_SERIAL,
                            json_mknumber (dbus_message_get_reply_serial
                                           (message)));

    // TRAP_NULL_STRING is not needed here if specification is correctly handled.
    if (flag & FLAG_PATH)
        json_append_member (message_node, DBUS_JSON_PATH,
                            json_mkstring (TRAP_NULL_STRING
                                           (dbus_message_get_path
                                            (message))));

    // Interface is optional for method calls.
    if (flag & FLAG_INTERFACE)
        json_append_member (message_node, DBUS_JSON_INTERFACE,
                            json_mkstring (TRAP_NULL_STRING
                                           (dbus_message_get_interface
                                            (message))));

    if (flag & FLAG_MEMBER)
        json_append_member (message_node, DBUS_JSON_MEMBER,
                            json_mkstring (TRAP_NULL_STRING
                                           (dbus_message_get_member
                                            (message))));

    // TRAP_NULL_STRING is not needed here if specification is correctly handled.
    if (flag & FLAG_ERROR_NAME)
        json_append_member (message_node, DBUS_JSON_ERROR_NAME,
                            json_mkstring (TRAP_NULL_STRING
                                           (dbus_message_get_error_name
                                            (message))));


    // ARGUMENTS
    DBusMessageIter args;
    if (!dbus_message_iter_init (message, &args))
    {
        // TODO: log this.
        fprintf (stderr, "Message has no arguments!\n");
        return NULL;
    }

    JsonNode* args_array = json_mkarray();
    do
    {
        json_append_element(args_array, args_mangler (&args));
    }
    while (dbus_message_iter_next (&args));

    json_append_member (message_node, DBUS_JSON_ARGS, args_array);

    return message_node;
}


/**
 * Bus Queries
 *
 * D-Bus provides a service with method that can return useful details about
 * current session / system. We use one function to centralize this: the
 * 'query' parameter will define what information we want.
 */
enum Queries
{
    QUERY_NONE,
    QUERY_LIST_NAMES,
    QUERY_ACTIVATABLE_NAMES,
    QUERY_GET_NAME_OWNER,
    QUERY_GET_CONNECTION_UNIX_USER,
    QUERY_GET_CONNECTION_UNIX_PROCESS_ID,
    QUERY_INTROSPECT
};

struct JsonNode*
query_bus (enum Queries query, char *parameter)
{
    DBusConnection *connection;
    DBusError error;

    DBusMessage *message = NULL;
    DBusMessageIter args;
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
    switch (query)
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
        return NULL;
    }

    if (message == NULL)
    {
        fprintf (stderr, "Message creation error.\n");
        return NULL;
    }

    if (need_parameter)
    {
        // Append parameter.
        dbus_message_iter_init_append (message, &args);
        if (!dbus_message_iter_append_basic
            (&args, DBUS_TYPE_STRING, &parameter))
        {
            fprintf (stderr, "Out Of Memory!\n");
            return NULL;
        }
    }

    // Send message and get a handle for a reply.
    // -1 is default timeout.
    if (!dbus_connection_send_with_reply (connection, message, &pending, -1))
    {
        fprintf (stderr, "Out Of Memory!\n");
         return NULL;
    }
    if (pending == NULL)
    {
        fprintf (stderr, "Pending Call Null\n");
        return NULL;
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
        return NULL;
    }

    // Free the pending message handle.
    dbus_pending_call_unref (pending);

    // Read the arguments.
    JsonNode *message_node = message_mangler (message);

    // free reply and close connection
    dbus_message_unref (message);
    dbus_connection_unref (connection);

    return message_node;
}


/**
 * Eavesdrop messages and store them internally. Parameter is a user-defined
 * filter.
 */

#define EAVES "eavesdrop=true"

void
spy (char *filter)
{
    DBusConnection *connection;
    DBusError error;

    DBusMessage *message = NULL;

    if (filter==NULL)
        filter = "";

    /**
     * Message filters.
     * Type is among:
     * -signal
     * -method_call
     * -method_return
     * -error
     */
    // Note: eavesdropping role is being prepended so that is can be turned off
    // by user in the filter.
    size_t filterlength = strlen (EAVES) + 1 + strlen (filter) + 1;
    char *eavesfilter = malloc (filterlength * sizeof (char));
    snprintf (eavesfilter, filterlength, "%s,%s", EAVES, filter);

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
        fprintf (stderr, "Bad filter: %s\n", error.message);
        puts("Filter syntax is as defined by D-Bus specification. Void filter will catch all messages. Example:");
        puts("  \"type='signal',sender='org.gnome.TypingMonitor',interface='org.gnome.TypingMonitor'\"");
        exit (1);
    }

    // TODO: put this line to log later.
    printf ("### Filter: %s\n", eavesfilter);
    
    // World available array containing all messages.
    message_array = json_mkarray();

    while (doneflag==0)
    {
        // Next available message. The use of the "dispatch" version handles the
        // queue for us. The timeout (in ms) is meant to find a balance between
        // signal response and CPU load).
        dbus_connection_read_write_dispatch (connection, 1000);
        message = dbus_connection_pop_message (connection);

        if (message != NULL)
        {
            JsonNode *message_node = message_mangler (message);
            json_append_element(message_array, message_node);
            json_print (message_node);
            dbus_message_unref (message);
        }
    }

    dbus_connection_unref (connection);
}

// D-Bus Message only.
// WARNING: manual free.
char* html_message(JsonNode* message)
{
    char* result;

    char* sec = json_stringify(json_find_member(message, DBUS_JSON_SEC), JSON_FORMAT_NONE);
    char* usec = json_stringify(json_find_member(message, DBUS_JSON_USEC), JSON_FORMAT_NONE);
    char* type = json_stringify(json_find_member(message, DBUS_JSON_TYPE), JSON_FORMAT_NONE);
    char* sender = json_stringify(json_find_member(message, DBUS_JSON_SENDER), JSON_FORMAT_NONE);
    char* destination = json_stringify(json_find_member(message, DBUS_JSON_DESTINATION), JSON_FORMAT_NONE);

    // sepsize is the space between the fields, and the last '+1' is for \0.
    size_t sepsize = 1;
    size_t result_size = strlen(sec)+sepsize
        + strlen(usec)+sepsize
        + strlen(type)+sepsize
        + strlen(sender)+sepsize
        + strlen(destination)+1;

    result = malloc(result_size * sizeof(char));
    snprintf( result, result_size, "%s %s %s %s %s", sec,usec,type,sender,destination);
    
    return result;
}

#if DAHSEE_UI_WEB != 0
static void
run_daemon ()
{
    // TODO: handle signal and eavesdrop on DBus.
    for (;;)
    {
        sleep (60);
    }

    return;
}
#endif

static void
print_help (const char *executable)
{
    printf ("Usage: %s [OPTION <ARG>] [<FILTER>]\n", executable);

    puts ("  -a        List activatable bus names.");
#ifdef DAHSEE_UI_WEB
    puts ("  -d        Daemonize.");
#endif
    puts ("  -f        Force overwriting when output file exists.");
    puts ("  -h        Print this help.");
    puts ("  -i NAME   Return introspection of NAME.");
    puts ("  -l        List registered bus names.");
    puts ("  -n NAME   Return unique name associated to NAME.");
    puts ("  -o FILE   Write output to FILE (default is stdout).");
    puts ("  -p        Return PID associated to NAME.");
    puts ("  -u NAME   Return UID who owns NAME.");
    puts ("  -v        Print version.");

    puts ("");
    puts ("Dahsee provides two main features:");
    puts (" * Queries to D-Bus session which let you get various details about applications, registered names, etc.");
    puts (" * Dahsee can list and filter all the messages travelling through D-Bus. This is the default behavior.");

    puts ("");
    puts ("FILTER syntax follows D-Bus specification. If FILTER is empty, all messages are caught. Further details on D-Bus and Dahsee are available from the man page. [see DAHSEE(1)]");
}

static void
print_version ()
{
    printf ("%s %s\n", APPNAME, VERSION);
    printf ("Copyright © %s %s\n", YEAR, AUTHOR);
    /* puts ("MIT License\n"); */
    /* puts ("This is free software: you are free to change and redistribute it.\n"); */
    /* puts ("There is NO WARRANTY, to the extent permitted by law.\n"); */
}

static void
nice_exit (int sig)
{
    doneflag=1;
    puts ("\nClosing...\n");
}


int
main (int argc, char **argv)
{
    // getopt() variables.
    int c;
    extern char *optarg;
    extern int optind;
    extern int optopt;

    enum Queries query = QUERY_NONE;
    int exclusive_opt = 0;

    // Catch SIGINT (Ctrl-C).
    struct sigaction act;
    act.sa_handler = nice_exit;
    act.sa_flags = 0;

    if ((sigemptyset (&act.sa_mask) == -1) ||
        (sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        return 1;
    }

#if DAHSEE_UI_WEB !=0
    // Fork variables.
    pid_t pid, sid;
    bool daemonize = false;
    while ((c = getopt (argc, argv, ":adfhH:i:I:ln:o:p:vu:")) != -1)
#else
    while ((c = getopt (argc, argv, ":afhH:i:I:ln:o:p:vu:")) != -1)
#endif
    {
        switch (c)
        {
        case 'a':
            exclusive_opt++;
            query = QUERY_ACTIVATABLE_NAMES;
            break;

#if DAHSEE_UI_WEB != 0
        case 'd':
            exclusive_opt++;
            daemonize = true;
            break;
#endif

        case 'f':
            option_force_overwrite = true;
            break;
        case 'H':
            option_output_format = FORMAT_XML;
            break;
        case 'h':
            print_help (argv[0]);
            return 0;
        case 'i':
            input_path = optarg;
            break;
        case 'I':
            exclusive_opt++;
            query = QUERY_INTROSPECT;
            break;
        case 'l':
            exclusive_opt++;
            query = QUERY_LIST_NAMES;
            break;
        case 'n':
            exclusive_opt++;
            query = QUERY_GET_NAME_OWNER;
            break;
        case 'o':
            output_path = optarg;
            break;
        case 'p':
            exclusive_opt++;
            query = QUERY_GET_CONNECTION_UNIX_PROCESS_ID;
            break;
        case 'v':
            print_version ();
            return 0;
        case 'u':
            exclusive_opt++;
            query = QUERY_GET_CONNECTION_UNIX_USER;
            break;
        case ':':
            // TODO: log this.
            printf ("-%c needs an argument.\n", optopt);
            return 1;
        case '?':
            // TODO: log this.
            printf ("Unknown argument %c.\n", optopt);
            return 1;
        default:
            print_help (argv[0]);
            return 0;
        }
    }

    if (exclusive_opt > 1)
    {
        // TODO: log this.
        puts("Mutually exclusive arguments were given.");
        return 1;
    }


    // TODO: check if useful.
    /* Set stdout to be unbuffered; this is basically so that if people
     * do dbus-monitor > file, then send SIGINT via Control-C, they
     * don't lose the last chunk of messages.
     */
    /* setvbuf (stdout, NULL, _IOLBF, 0); */

    // TODO: combine import file with web and stats.
    /* source_node = json_import(optarg); */
    /* json_print(source_node); */
            
    // TODO: handle output formats.
    /* source_node = json_import(optarg); */
    /* source_text=html_message(source_node); */
    /* printf("%s\n",source_text); */
    /* json_delete(source_node); */
    /* free(source_text); */
    /* return 0; */


    if (query != QUERY_NONE)
    {
        JsonNode* source_node;

        // Queries
        switch (query)
        {
        case QUERY_LIST_NAMES:
            json_print(query_bus (QUERY_LIST_NAMES, NULL));
            break;
        case QUERY_ACTIVATABLE_NAMES:
            json_print(query_bus (QUERY_ACTIVATABLE_NAMES, NULL));
            break;
        case QUERY_GET_NAME_OWNER:
            json_print(query_bus (QUERY_GET_NAME_OWNER, optarg));
            break;
        case QUERY_GET_CONNECTION_UNIX_USER:
            json_print(query_bus (QUERY_GET_CONNECTION_UNIX_USER, optarg));
            break;
        case QUERY_GET_CONNECTION_UNIX_PROCESS_ID:
            json_print(query_bus (QUERY_GET_CONNECTION_UNIX_PROCESS_ID, optarg));
            break;
        case QUERY_INTROSPECT:
            // TODO: two possible result (use option):

            // XML-to-JSON.
            source_node = json_find_member(
                json_find_element(
                    json_find_member(
                        query_bus (QUERY_INTROSPECT, optarg), "args" ),0), "value");

            json_print(dbus_xml_parser(source_node->string_));

            // Raw XML.
            source_node = json_find_member(
                json_find_element(
                    json_find_member(
                        query_bus (QUERY_INTROSPECT, optarg), "args" ),0), "value");

            printf ("%s\n", source_node->string_);

            return 0;
        default:
            break;
        }
    }
#if DAHSEE_UI_WEB != 0
    // Daemon
    else if (daemonize == true)
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
#endif
    else
    {
        // Eavesdrop bus messages. Default bhaviour.

        // WARNING: considering that all remaining args are arguments may be
        // POSIXLY_INCORRECT.  TODO: check behaviour of when POSIXLY_CORRECT is set.
        char * filter;
        if (argc-optind != 1)
            filter = NULL;
        else
            filter = argv[optind];

        /* int i; */
        /* for (i = optind; i < argc; i++) */
        /* { */
        /*     printf ("%s\n", argv[i]); */
        /* } */
        // TODO: add filters
        spy(filter);
    }


    // Clean global stuff.
    if (message_array != NULL)
        json_delete (message_array);

    if (output != NULL && output != stdout)
        fclose(output);

    if (input != NULL)
        fclose(output);

    return 0;
}
