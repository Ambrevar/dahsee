/*******************************************************************************
Dahsee - a D-Bus monitoring tool

Copyright © 2012 Pierre Neidhardt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

/*
QUESTIONS:
Why does "ListNames" method on DBUS_SERVICE_DBUS,DBUS_PATH_DBUS does not list org.freedesktop.DBus ?

-Test if object name MUST begin with "/"
-Test if Interface can be anything... Depends?
-Get method call args
-Test object hierarchy (if parent gets it; then child too, or vice versa)
-how should react deamon argument?

TODO: check if all message_mangler calls get properly cleaned.
*/

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

/* Optional UI support */
#if DAHSEE_UI_WEB != 0
#include "ui_web.h"
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
/* Options */

/* TODO: check if output is always properly closed on exit. */

/* This flag is used to shutdown properly when SIGINT is caught.  Since some */
/* function (e.g. spy()) run an inifite loop, this is the only way to let them */
/* know when it is time to quit. */
static volatile sig_atomic_t doneflag = 0;
static volatile sig_atomic_t spyflag = 0;

/* Paths and file descriptors for output and logfile. */
/* TODO: handle input. */
/* TODO: Use freopen() to change output and logfile. Then perror() can be used safely. */
static FILE *output = NULL;
static FILE *logfile = NULL;
static FILE *input = NULL;
static const char *output_path = NULL;
/* static const char *input_path = NULL; */
static const char *logfile_path = NULL;

/* Contains the whole bunch of messages caught using spy(); */
JsonNode *message_array;

/* Contains HTML info. */
char *html_message;

/* Output options. */
enum OutputFormat {
	FORMAT_JSON,
	FORMAT_PROFILE,
	FORMAT_XML
};
/* With this variable we can set the text format / structure of reports. */
/* static int option_output_format = FORMAT_JSON; */

/* If the output is set to an existing file, it will not overwrite it by default */
/* (outputting to stdout instead). We can use an option to force overwriting. */
static bool option_force_overwrite = false;

/* Specify the indentation in JSON output. */
#define JSON_FORMAT "  "
#define JSON_FORMAT_NONE ""


/******************************************************************************/
/* Functions */

/*
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

    // WARNING: it is important not to forget the terminating null byte! We add
    // +1 to 'pos' for that.

    // File buffering.
    fseek(inputfile, 0, SEEK_END);
    pos = ftell(inputfile) + 1;
    fseek(inputfile, 0, SEEK_SET);

    inputstring = malloc(pos*sizeof(char));
    fread(input, sizeof(char), pos, inputfile);
    fclose(inputfile);
    inputstring[pos]='\0';

    // Parsing.
    JsonNode* imported_json=NULL;
    imported_json = json_decode(inputstring);

    free(inputstring);
    return imported_json;
}
*/

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

void json_print(JsonNode *message) {

	char *tmp = json_stringify(message, JSON_FORMAT);
	fwrite(tmp, sizeof (char), strlen(tmp), output);
	fwrite("\n", sizeof (char), 1, output);

	free(tmp);
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

struct xml_dict {
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
	{ 'y', "Byte" },
	{ 'b', "Boolean" },
	{ 'n', "Int16" },
	{ 'q', "Uint16" },
	{ 'i', "Int32" },
	{ 'u', "Uint32" },
	{ 'x', "Int64" },
	{ 't', "Uint64" },
	{ 'd', "Double" },
	{ 's', "String" },
	{ 'o', "Object Path" },
	{ 'g', "Signature" },
	{ 'a', "Array of" },
	{ '(', "Struct (" },
	{ ')', ")" },
	{ 'v', "Variant" },
	{ '{', "Dict entry {" },
	{ '}', "}" },
	{ 'h', "Unix fd" },
	{ '\0', NULL }
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


/* TODO: test without reentrency support. */
struct JsonNode *dbus_xml_parser(char *source) {
	struct JsonNode *result = json_mkarray();
	struct JsonNode *current_node;

	/* Node where current_node is being added. */
	struct JsonNode *last_node_ptr;

	/* The two variables are used to get access to the appropriate array. */
	struct JsonNode *last_member_ptr = NULL;
	struct JsonNode *last_interface_ptr = NULL;

	char *token;

	/* Name, Type, Direction */
	char *property;
	/* Property value */
	char *value;

	char *saveptr1, *saveptr2, *saveptr3;

	/* Skip header on first iteration. */
	strtok_r(source, XML_DELIMS_NODES, &saveptr1);

	/* Node slice */
	for (;; ) {
		token = strtok_r(NULL, XML_DELIMS_NODES, &saveptr1);

		if (token == NULL) {
			break;
		}

		/* Skip space between XML nodes. */
		/* WARNING: this works if there is ONLY one node per line, and '\n' */
		/* right after it. */
		if (token[0] == '\n') {
			continue;
		}

		/* Skip XML closure. */
		if (token[0] == '/') {
			continue;
		}

		/* Node Type */
		token = strtok_r(token, XML_DELIMS_PROPERTIES, &saveptr2);

		/* Should never happen (except if node has spaces only). */
		if (token == NULL) {
			continue;
		}

		/* Main switch. WARNING: this part is not safe at all if input is badly */
		/* formatted. */
		if (strcmp(token, XML_NODE_OBJECT) == 0) {
			last_node_ptr = result;
		} else if (strcmp(token, XML_NODE_INTERFACE) == 0) {
			last_node_ptr = result->children.tail;

			if (json_find_member(last_node_ptr, XML_NODE_INTERFACE) == NULL) {
				json_append_member(last_node_ptr, XML_NODE_INTERFACE, json_mkarray());
			}
			last_node_ptr = json_find_member(last_node_ptr, XML_NODE_INTERFACE);
			last_interface_ptr = last_node_ptr;
		} else if (strcmp(token, XML_NODE_METHOD) == 0) {
			last_node_ptr = last_interface_ptr->children.tail;

			if (json_find_member(last_node_ptr, XML_NODE_METHOD) == NULL) {
				json_append_member(last_node_ptr, XML_NODE_METHOD, json_mkarray());
			}
			last_node_ptr = json_find_member(last_node_ptr, XML_NODE_METHOD);
			last_member_ptr = last_node_ptr;
		} else if (strcmp(token, XML_NODE_SIGNAL) == 0) {
			last_node_ptr = last_interface_ptr->children.tail;

			if (json_find_member(last_node_ptr, XML_NODE_SIGNAL) == NULL) {
				json_append_member(last_node_ptr, XML_NODE_SIGNAL, json_mkarray());
			}
			last_node_ptr = json_find_member(last_node_ptr, XML_NODE_SIGNAL);
			last_member_ptr = last_node_ptr;
		} else if (strcmp(token, XML_NODE_ARG) == 0) {
			last_node_ptr = last_member_ptr->children.tail;

			if (json_find_member(last_node_ptr, XML_NODE_ARG) == NULL) {
				json_append_member(last_node_ptr, XML_NODE_ARG, json_mkarray());
			}
			last_node_ptr = json_find_member(last_node_ptr, XML_NODE_ARG);
		}
		/* Should never happen if input is as expected. */
		else {
			fprintf(logfile, "ERROR: could not recognize token (%s).\n", token);
			break;
		}

		current_node = json_mkobject();

		/* Properties */
		for (;; ) {
			token = strtok_r(NULL, XML_DELIMS_PROPERTIES, &saveptr2);

			if (token == NULL) {
				break;
			}

			property = strtok_r(token, XML_DELIMS_VALUES, &saveptr3);
			if (property == NULL) { /* Shoud never happen. */
				break;
			}
			/* Skip closing '/'. */
			if (property[0] == '/') {
				continue;
			}

			value = strtok_r(NULL, XML_DELIMS_VALUES, &saveptr3);
			if (value == NULL) {
				continue;
			}

			if (strcmp(property, XML_PROPERTY_NAME) == 0) {
				json_append_member(current_node, XML_PROPERTY_NAME, json_mkstring(value));
			} else if (strcmp(property, XML_PROPERTY_DIRECTION) == 0) {
				json_append_member(current_node, XML_PROPERTY_DIRECTION, json_mkstring(value));
			} else if (strcmp(property, XML_PROPERTY_TYPE) == 0) {
				size_t i, j;
				size_t arg_sym_size = strlen(value);
				char **array_ptr = malloc(arg_sym_size * sizeof (char *));
				char *arg_string;
				size_t arg_string_size = 0;

				for (i = 0; i < arg_sym_size; i++) {
					for (j = 0; dbus_value_type_table[j].key != '\0'; j++) {
						if (dbus_value_type_table[j].key == value[i]) {
							arg_string_size += strlen(dbus_value_type_table[j].value) + 1;
							array_ptr[i] = dbus_value_type_table[j].value;
							break;
						}
					}

					/* Char not found in table. */
					if (dbus_value_type_table[j].key == '\0') {
						fprintf(logfile, "ERROR: Type char (%c) not known.\n", value[i]);
						break;
					}
				}

				arg_string = calloc(arg_string_size, sizeof (char));
				strcat(arg_string, array_ptr[0]);
				for (i = 1; i < arg_sym_size; i++) {
					strcat(arg_string, " ");
					strcat(arg_string, array_ptr[i]);
				}

				json_append_member(current_node, XML_PROPERTY_TYPE, json_mkstring(arg_string));
			}
			/* Should never happen. */
			else {
				fprintf(logfile, "ERROR: could not recognize token (%s).\n", token);
				break;
			}

		} /* End of properties. */
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
/* TODO: get arg name. */
struct JsonNode *args_mangler(DBusMessageIter *args) {
	struct JsonNode *args_node = json_mkobject();

	int type = dbus_message_iter_get_arg_type(args);

	/* TODO: check if useful to log it or not. I guess so. See below. */
	/* if (type == DBUS_TYPE_INVALID) */
	/*     break; */

	switch (type) {
	case DBUS_TYPE_STRING:
	{
		char *value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("string"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mkstring(value));
		break;
	}

	case DBUS_TYPE_SIGNATURE:
	{
		char *value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("signature"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mkstring(value));
		break;
	}

	/* TODO: object_path or path? */
	case DBUS_TYPE_OBJECT_PATH:
	{
		char *value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("object_path"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mkstring(value));
		break;
	}

	case DBUS_TYPE_INT16:
	{
		dbus_int16_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("int16"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	case DBUS_TYPE_UINT16:
	{
		dbus_uint16_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("uint16"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	case DBUS_TYPE_INT32:
	{
		dbus_int32_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("int32"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	case DBUS_TYPE_UINT32:
	{
		dbus_uint32_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("uint32"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	/* TODO: check if 64 bit works properly. */
	case DBUS_TYPE_INT64:
	{
		dbus_int64_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("int64"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	case DBUS_TYPE_UINT64:
	{
		dbus_uint64_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("uint64"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	case DBUS_TYPE_DOUBLE:
	{
		double value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("double"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	case DBUS_TYPE_BYTE:
	{
		unsigned char value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("byte"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	/* TODO: bool or boolean? */
	case DBUS_TYPE_BOOLEAN:
	{
		dbus_bool_t value;
		dbus_message_iter_get_basic(args, &value);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("bool"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			json_mknumber(value));
		break;
	}

	/* TODO: Needs testing. */
	case DBUS_TYPE_VARIANT:
	{
		DBusMessageIter subargs;
		dbus_message_iter_recurse(args, &subargs);
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("variant"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE,
			args_mangler(&subargs));
		break;
	}

	/* TODO: Needs testing. Seems to be ok. */

	/* Since D-Bus array elements are all of the same type, we store this */
	/* information along 'type' and 'value', 'value' being a JSON array with the */
	/* values. That's why we need to strip the type information from the */
	/* recursive call. But since JSON does not allow multiple parents, with must */
	/* remove the parent first. */
	case DBUS_TYPE_ARRAY:
	{
		DBusMessageIter subargs;
		dbus_message_iter_recurse(args, &subargs);
		struct JsonNode *array = json_mkarray();
		struct JsonNode *arraytype;
		struct JsonNode *arrayvalue;
		struct JsonNode *arrayarg;

		arraytype = json_find_member(args_mangler(&subargs), DBUS_JSON_ARG_TYPE);
		json_remove_from_parent(arraytype);

		while (dbus_message_iter_get_arg_type(&subargs) !=
			DBUS_TYPE_INVALID) {
			arrayarg = args_mangler(&subargs);
			arrayvalue = json_find_member(arrayarg, DBUS_JSON_ARG_VALUE);
			json_remove_from_parent(arrayvalue);
			json_append_element(array, arrayvalue);

			/* TODO: CHECK if no JSON element is lost. */
			json_delete(arrayarg);
			dbus_message_iter_next(&subargs);
		}

		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("array"));
		json_append_member(args_node, DBUS_JSON_ARG_ARRAYTYPE, arraytype);
		json_append_member(args_node, DBUS_JSON_ARG_VALUE, array);

		break;
	}

	/* TODO: check if JSON structure is right here. */
	case DBUS_TYPE_DICT_ENTRY:
	{
		DBusMessageIter subargs;
		dbus_message_iter_recurse(args, &subargs);

		struct JsonNode *dict = json_mkarray();
		json_append_element(dict, args_mangler(&subargs));
		dbus_message_iter_next(&subargs);
		json_append_element(dict, args_mangler(&subargs));

		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("dict_entry"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE, dict);
		break;
	}

	case DBUS_TYPE_STRUCT:
	{
		DBusMessageIter subargs;
		dbus_message_iter_recurse(args, &subargs);
		struct JsonNode *structure = json_mkarray();

		while (dbus_message_iter_get_arg_type(&subargs) !=
			DBUS_TYPE_INVALID) {
			json_append_element(structure, args_mangler(&subargs));
			dbus_message_iter_next(&subargs);
		}

		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("struct"));
		json_append_member(args_node, DBUS_JSON_ARG_VALUE, structure);
		break;
	}

	/* TODO: write something else ? */
	case DBUS_TYPE_UNIX_FD:
	{
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("unix_fd"));
		break;
	}

	case DBUS_TYPE_INVALID:
	{
		json_append_member(args_node, DBUS_JSON_ARG_TYPE, json_mkstring("invalid"));
		break;
	}

	default:
		fprintf(logfile, "WARNING: Type (%c) out of specification!\n", type);
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

enum Flags {
	FLAG_SERIAL = 1,
	FLAG_REPLY_SERIAL = 2,
	FLAG_PATH = 4,
	FLAG_INTERFACE = 8,
	FLAG_MEMBER = 32,
	FLAG_ERROR_NAME = 64
};

/* WARNING: manual free with json_delete(node). */
struct JsonNode *message_mangler(DBusMessage *message) {
	struct JsonNode *message_node = json_mkobject();
	enum Flags flag = 0;

	/* TIME */
	struct timeval time_machine;
	struct tm *time_human;

	/* TODO: check APPLE support since it is supposed to have a different struct */
	/* timeval. */
	if (gettimeofday(&time_machine, NULL) < 0) {
		fprintf(logfile, "ERROR: Could not get timestamp.\n");
		return NULL;
	}

	/* TODO: use argv parameter to request human readable time and local/UTC. */
	/* Use "localtime" or "gmtime" to get date and time from the elapsed seconds */
	/* since epoch. */
	/* time_human = localtime( &(time_machine.tv_sec) ); */
	time_human = gmtime(&(time_machine.tv_sec));

	json_append_member(message_node, DBUS_JSON_SEC,
		json_mknumber(time_machine.tv_sec));
	json_append_member(message_node, DBUS_JSON_USEC,
		json_mknumber(time_machine.tv_usec));

	/* TODO: make this field optional. */
	struct JsonNode *time_node = json_mkobject();
	json_append_member(time_node, DBUS_JSON_HOUR,
		json_mknumber(time_human->tm_hour));
	json_append_member(time_node, DBUS_JSON_MINUTE, json_mknumber(time_human->tm_min));
	json_append_member(time_node, DBUS_JSON_SECOND, json_mknumber(time_human->tm_sec));
	json_append_member(message_node, DBUS_JSON_TIME_HUMAN, time_node);

	/* TYPE */
	switch (dbus_message_get_type(message)) {
	/* TODO: check if serial is different / needed for error and method return. */
	case DBUS_MESSAGE_TYPE_ERROR:
		json_append_member(message_node, DBUS_JSON_TYPE,
			json_mkstring(DBUS_JSON_ERROR));
		flag = FLAG_SERIAL | FLAG_ERROR_NAME | FLAG_REPLY_SERIAL;
		break;

	case DBUS_MESSAGE_TYPE_METHOD_CALL:
		json_append_member(message_node, DBUS_JSON_TYPE,
			json_mkstring(DBUS_JSON_METHOD_CALL));
		flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER;
		break;

	case DBUS_MESSAGE_TYPE_METHOD_RETURN:
		json_append_member(message_node, DBUS_JSON_TYPE,
			json_mkstring(DBUS_JSON_METHOD_RETURN));
		flag = FLAG_SERIAL | FLAG_REPLY_SERIAL;
		break;

	case DBUS_MESSAGE_TYPE_SIGNAL:
		json_append_member(message_node, DBUS_JSON_TYPE,
			json_mkstring(DBUS_JSON_SIGNAL));
		flag = FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER;
		break;

	default:
		json_append_member(message_node, DBUS_JSON_TYPE,
			json_mkstring(DBUS_JSON_UNKNOWN));
		flag =
			FLAG_SERIAL | FLAG_PATH | FLAG_INTERFACE | FLAG_MEMBER |
			FLAG_ERROR_NAME | FLAG_REPLY_SERIAL;
		break;
	}

	/* TODO: get unique name for sender and destination instead of well-known */
	/* names. Well-known names should be an option. */

	/* SENDER */
	json_append_member(message_node, DBUS_JSON_SENDER,
		json_mkstring(TRAP_NULL_STRING
			(dbus_message_get_sender(message))));

	/* DESTINATION */
	json_append_member(message_node, DBUS_JSON_DESTINATION,
		json_mkstring(TRAP_NULL_STRING
			(dbus_message_get_destination
			(message))));

	/* FLAGS */
	if (flag & FLAG_SERIAL) {
		json_append_member(message_node, DBUS_JSON_SERIAL,
			json_mknumber(dbus_message_get_serial
				(message)));
	}

	if (flag & FLAG_REPLY_SERIAL) {
		json_append_member(message_node, DBUS_JSON_REPLY_SERIAL,
			json_mknumber(dbus_message_get_reply_serial
				(message)));
	}

	/* TRAP_NULL_STRING is not needed here if specification is correctly handled. */
	if (flag & FLAG_PATH) {
		json_append_member(message_node, DBUS_JSON_PATH,
			json_mkstring(TRAP_NULL_STRING
				(dbus_message_get_path
				(message))));
	}

	/* Interface is optional for method calls. */
	if (flag & FLAG_INTERFACE) {
		json_append_member(message_node, DBUS_JSON_INTERFACE,
			json_mkstring(TRAP_NULL_STRING
				(dbus_message_get_interface
				(message))));
	}

	if (flag & FLAG_MEMBER) {
		json_append_member(message_node, DBUS_JSON_MEMBER,
			json_mkstring(TRAP_NULL_STRING
				(dbus_message_get_member
				(message))));
	}

	/* TRAP_NULL_STRING is not needed here if specification is correctly handled. */
	if (flag & FLAG_ERROR_NAME) {
		json_append_member(message_node, DBUS_JSON_ERROR_NAME,
			json_mkstring(TRAP_NULL_STRING
				(dbus_message_get_error_name
				(message))));
	}


	/* ARGUMENTS */
	DBusMessageIter args;
	if (dbus_message_iter_init(message, &args)) {
		/* fprintf (logfile, "ERROR: Message has no arguments.\n"); */
		/* return NULL; */

		JsonNode *args_array = json_mkarray();
		do {
			json_append_element(args_array, args_mangler(&args));
		} while (dbus_message_iter_next(&args));

		json_append_member(message_node, DBUS_JSON_ARGS, args_array);
	}

	return message_node;
}


/**
 * Bus Queries
 *
 * D-Bus provides a service with method that can return useful details about
 * current session / system. We use one function to centralize this: the
 * 'query' parameter will define what information we want.
 */
enum Queries {
	QUERY_NONE,
	QUERY_LIST_NAMES,
	QUERY_ACTIVATABLE_NAMES,
	QUERY_GET_NAME_OWNER,
	QUERY_GET_CONNECTION_UNIX_USER,
	QUERY_GET_CONNECTION_UNIX_PROCESS_ID,
	QUERY_INTROSPECT
};

struct JsonNode *query_bus(enum Queries query, const char *parameter) {
	DBusConnection *connection;
	DBusError error;

	DBusMessage *message = NULL;
	DBusMessageIter args;
	bool need_parameter = false;

	DBusPendingCall *pending;

	/* Init */
	dbus_error_init(&error);

	connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		fprintf(logfile, "ERROR: Connection Error (%s).\n", error.message);
		dbus_error_free(&error);
	}
	if (connection == NULL) {
		exit(1);
	}


	/* Create a new method call. */
	switch (query) {
	case QUERY_LIST_NAMES:
	{
		message = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS,
				DBUS_INTERFACE_DBUS,
				"ListNames");
		break;
	}

	case QUERY_ACTIVATABLE_NAMES:
	{
		message = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS,
				DBUS_INTERFACE_DBUS,
				"ListActivatableNames");
		break;
	}

	case QUERY_GET_NAME_OWNER:
	{
		message = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS,
				DBUS_INTERFACE_DBUS,
				"GetNameOwner");
		need_parameter = true;
		break;
	}

	case QUERY_GET_CONNECTION_UNIX_USER:
	{
		message = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS,
				DBUS_INTERFACE_DBUS,
				"GetConnectionUnixUser");
		need_parameter = true;
		break;
	}

	case QUERY_GET_CONNECTION_UNIX_PROCESS_ID:
	{
		message = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS,
				DBUS_INTERFACE_DBUS,
				"GetConnectionUnixProcessID");
		need_parameter = true;
		break;
	}

	case QUERY_INTROSPECT:
	{
		/* TODO: Need to check if parameter is a valid busname. */
		message = dbus_message_new_method_call(parameter,
				DBUS_PATH_DBUS,
				"org.freedesktop.DBus.Introspectable",
				"Introspect");
		break;
	}

	default:
		fprintf(logfile, "ERROR: Query unrecognized.\n");
		return NULL;
	}

	if (message == NULL) {
		fprintf(logfile, "ERROR: Message creation error.\n");
		return NULL;
	}

	if (need_parameter) {
		/* Append parameter. */
		dbus_message_iter_init_append(message, &args);
		if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parameter)) {
			fprintf(logfile, "ERROR: Out Of Memory!\n");
			return NULL;
		}
	}

	/* Send message and get a handle for a reply. */
	/* -1 is default timeout. */
	if (!dbus_connection_send_with_reply(connection, message, &pending, -1)) {
		fprintf(logfile, "ERROR: Out Of Memory!\n");
		return NULL;
	}
	if (pending == NULL) {
		fprintf(logfile, "ERROR: Pending Call Null\n");
		return NULL;
	}

	/* Force sending request without waiting. */
	dbus_connection_flush(connection);

	/* Free message. */
	dbus_message_unref(message);

	/* Block until we recieve a reply. */
	dbus_pending_call_block(pending);

	/* Get the reply message. */
	message = dbus_pending_call_steal_reply(pending);
	if (message == NULL) {
		fprintf(logfile, "ERROR: Reply Null\n");
		return NULL;
	}

	/* Free the pending message handle. */
	dbus_pending_call_unref(pending);

	/* Read the arguments. */
	JsonNode *message_node = message_mangler(message);

	/* free reply and close connection */
	dbus_message_unref(message);
	dbus_connection_unref(connection);

	return message_node;
}



/* D-Bus Messages only. */
/* WARNING: manual free. */
/* TODO: add 'error_name' field. */

/**
   <tr>
   <td>1344361353.210869</td>
   <td>17:42:33</td>
   <td>method_call</td>
   <td>:1.68</td>
   <td>org.freedesktop.DBus</td>
   <td>2</td>
   <td>-</td>
   <td>/org/freedesktop/DBus</td>
   <td>org.freedesktop.DBus</td>
   <td>Addmatch</td>
   <td>String:eavesdrop=true</td>
   </tr>
 */
#define MEM_CHUNK 32768
#define HTML_EMPTY_FIELD "-"
#define HTML_DATA_LIMIT 255
char *message_to_html(JsonNode *message) {
	char *result = NULL;
	size_t result_size = 0;
	size_t result_size_real = 0;

	JsonNode *node = message;

	char *html_row_begin = "\n<tr>";
	size_t html_row_begin_len = strlen(html_row_begin);
	char *html_row_end = "</tr>\n";
	size_t html_row_end_len = strlen(html_row_end);
	char *html_column_begin = "<td>";
	size_t html_column_begin_len = strlen(html_column_begin);
	char *html_column_end = "</td>\n";
	size_t html_column_end_len = strlen(html_column_end);

	while (node != NULL && node->tag == JSON_ARRAY) {
		node = json_first_child(node);
	}

	while (node != NULL) {
		/* Get machine time. */
		char *sec = json_stringify(json_find_member(node, DBUS_JSON_SEC), JSON_FORMAT_NONE);
		char *usec = json_stringify(json_find_member(node, DBUS_JSON_USEC), JSON_FORMAT_NONE);

		/* The '+1' is for the separator. */
		char *mtime = malloc((strlen(sec) + strlen(usec) + 1 + 1) * sizeof (char));
		strcpy(mtime, sec);
		strcat(mtime, ".");
		strcat(mtime, usec);

		/* Get Human time. */
		JsonNode *htime_node = json_find_member(node, DBUS_JSON_TIME_HUMAN);

		/* Get time in 2-digits format. */
		/* TODO: stupid code. */
		char hour[3];
		long hour_val = (long)json_find_member(htime_node, DBUS_JSON_HOUR)->number_;
		hour[0] = '0' + hour_val / 10;
		hour[1] = '0' + hour_val % 10;
		hour[2] = '\0';
		char minute[3];
		long minute_val = (long)json_find_member(htime_node, DBUS_JSON_MINUTE)->number_;
		minute[0] = '0' + (minute_val / 10);
		minute[1] = '0' + (minute_val % 10);
		minute[2] = '\0';
		char second[3];
		long second_val = (long)json_find_member(htime_node, DBUS_JSON_SECOND)->number_;
		second[0] = '0' + second_val / 10;
		second[1] = '0' + second_val % 10;
		second[2] = '\0';

		/* The '+2' is for the separator. */
		char *htime = malloc((strlen(hour) + strlen(minute) + strlen(second) + 2 + 1) * sizeof (char));
		strcpy(htime, hour);
		strcat(htime, ":");
		strcat(htime, minute);
		strcat(htime, ":");
		strcat(htime, second);


		/* Note that strings returned from string_ should not be freed. */

		char *type = json_find_member(node, DBUS_JSON_TYPE)->string_;
		char *sender = json_find_member(node, DBUS_JSON_SENDER)->string_;
		char *destination = json_find_member(node, DBUS_JSON_DESTINATION)->string_;

		/* Strings returned from stringify should be freed. */
		JsonNode *temp;

		char *serial = NULL;
		temp = json_find_member(node, DBUS_JSON_SERIAL);
		if (temp != NULL) {
			serial = json_stringify(temp, JSON_FORMAT_NONE);
		} else {
			serial = HTML_EMPTY_FIELD;
		}

		char *reply_serial = NULL;
		temp = json_find_member(node, DBUS_JSON_REPLY_SERIAL);
		if (temp != NULL) {
			reply_serial = json_stringify(temp, JSON_FORMAT_NONE);
		} else {
			reply_serial = HTML_EMPTY_FIELD;
		}

		char *path = NULL;
		temp = json_find_member(node, DBUS_JSON_PATH);
		if (temp != NULL) {
			path = temp->string_;
		} else {
			path = HTML_EMPTY_FIELD;
		}

		char *interface = NULL;
		temp = json_find_member(node, DBUS_JSON_INTERFACE);
		if (temp != NULL) {
			interface = temp->string_;
		} else {
			interface = HTML_EMPTY_FIELD;
		}

		char *member = NULL;
		temp = json_find_member(node, DBUS_JSON_MEMBER);
		if (temp != NULL) {
			member = temp->string_;
		} else {
			member = HTML_EMPTY_FIELD;
		}

		/* TODO: args. */
		/* Separator: ', ' */
		char *args = NULL;
		size_t args_len = 1;
		args = malloc(1);
		args[0] = '\0';
		temp = json_find_member(node, DBUS_JSON_ARGS);
		if (temp != NULL) {
			JsonNode *arg_iter = json_first_child(temp);
			JsonNode *arg_iter_element;
			json_foreach(arg_iter, temp) {
				/* '+1' for the separator. */
				arg_iter_element = json_find_member(arg_iter, DBUS_JSON_ARG_TYPE);
				args_len += strlen(arg_iter_element->string_) + 1;
				args = realloc(args, args_len * sizeof (char));
				strcat(args, arg_iter_element->string_);

				strcat(args, "=");

				arg_iter_element = json_find_member(arg_iter, DBUS_JSON_ARG_VALUE);
				char *temp_str = json_stringify(arg_iter_element, JSON_FORMAT_NONE);
				size_t temp_str_len = strlen(temp_str);
				if (temp_str_len > HTML_DATA_LIMIT) {
					char *buf = malloc((HTML_DATA_LIMIT + 1) * sizeof (char));
					strncpy(buf, temp_str, HTML_DATA_LIMIT - 3);
					strcpy(buf + 252, "...");
					free(temp_str);
					temp_str = buf;

				}

				args_len += temp_str_len + 1;
				args = realloc(args, args_len * sizeof (char));
				strcat(args, temp_str);
				free(temp_str);

				strcat(args, " ");
			}
		} else {
			args = HTML_EMPTY_FIELD;
		}


		/* Note: do not forget +1 for the null terminating byte. */
		result_size += html_row_begin_len + html_row_end_len + 1;

		result_size += html_column_begin_len + html_column_end_len + strlen(mtime);
		result_size += html_column_begin_len + html_column_end_len + strlen(htime);
		result_size += html_column_begin_len + html_column_end_len + strlen(type);
		result_size += html_column_begin_len + html_column_end_len + strlen(sender);
		result_size += html_column_begin_len + html_column_end_len + strlen(destination);

		result_size += html_column_begin_len + html_column_end_len + strlen(serial);
		result_size += html_column_begin_len + html_column_end_len + strlen(reply_serial);
		result_size += html_column_begin_len + html_column_end_len + strlen(path);
		result_size += html_column_begin_len + html_column_end_len + strlen(interface);
		result_size += html_column_begin_len + html_column_end_len + strlen(member);

		result_size += html_column_begin_len + html_column_end_len + strlen(args);

		/* TODO: why does the following not work ? */
		/* result = realloc( result, result_size * sizeof(char)); */
		if (result_size_real <= result_size) {
			result_size_real += MEM_CHUNK;
			result = realloc(result, result_size_real * sizeof (char));
		}

		if (result == NULL) {
			perror("HTML message memory error");
		}

		strcat(result, html_row_begin);

		strcat(result, html_column_begin);
		strcat(result, mtime);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, htime);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, type);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, sender);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, destination);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, serial);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, reply_serial);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, path);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, interface);
		strcat(result, html_column_end);

		strcat(result, html_column_begin);
		strcat(result, member);
		strcat(result, html_column_end);

		fprintf(logfile, "[%s]\n", args);

		strcat(result, html_column_begin);
		strcat(result, args);
		strcat(result, html_column_end);

		strcat(result, html_row_end);

		/* TEST: */
		/* fprintf (stderr, "%s\n", result); */

		/* TODO: Clean properly. */
		free(mtime);
		free(sec);
		free(usec);

		free(htime);

		node = node->next;
	}

	return result;
}



/**
 * Eavesdrop messages and store them internally. Parameter is a user-defined
 * filter.
 */

#define EAVES "eavesdrop=true"
#define LIVE_OUTPUT_OFF 0
#define LIVE_OUTPUT_ON 1

void spy(char *filter, int opt) {
	DBusConnection *connection;
	DBusError error;

	DBusMessage *message = NULL;

	if (filter == NULL) {
		filter = "";
	}

	/**
	 * Message filters.
	 * Type is among:
	 * -signal
	 * -method_call
	 * -method_return
	 * -error
	 */
	/* Note: eavesdropping role is being prepended so that is can be turned off */
	/* by user in the filter. */
	size_t filterlength = strlen(EAVES) + 1 + strlen(filter) + 1;
	char *eavesfilter = malloc(filterlength * sizeof (char));
	snprintf(eavesfilter, filterlength, "%s,%s", EAVES, filter);

	/* Init */
	dbus_error_init(&error);

	connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		fprintf(logfile, "ERROR: Connection Error (%s)", error.message);
		dbus_error_free(&error);
	}
	if (connection == NULL) {
		exit(1);
	}

	dbus_bus_add_match(connection, eavesfilter, &error);
	/* TODO: useless here? */
	dbus_connection_flush(connection);

	if (dbus_error_is_set(&error)) {
		fprintf(logfile, "ERROR: Bad filter (%s).\n", error.message);
		fprintf(logfile, "==> Filter syntax is as defined by D-Bus specification. Void filter will catch all messages.\n==> Example:\n");
		fprintf(logfile, "==>    \"type='signal',sender='org.gnome.TypingMonitor',interface='org.gnome.TypingMonitor'\"\n");
		return;
	}

	fprintf(logfile, "NOTE: Filter in use is %s\n", eavesfilter);

	/* World available array containing all messages. */
	if (message_array != NULL) {
		json_delete(message_array);
	}
	message_array = json_mkarray();

	/* TEST: */
	/* int i; */
	/* for (i = 0; i<3; i++) */
	while (doneflag == 0) {
		/* Next available message. The use of the "dispatch" version handles the */
		/* queue for us. The timeout (in ms) is meant to find a balance between */
		/* signal response and CPU load). */
		/* dbus_connection_read_write_dispatch (connection, -1); */
		dbus_connection_read_write_dispatch(connection, 1000);
		message = dbus_connection_pop_message(connection);

		if (message != NULL) {
			JsonNode *message_node = message_mangler(message);
			json_append_element(message_array, message_node);

			if (opt == LIVE_OUTPUT_ON) {
				json_print(message_node);
			}

			dbus_message_unref(message);
		}
	}
	doneflag = 0;

	if (opt == LIVE_OUTPUT_OFF) {
		html_message = message_to_html(message_array);
	}

	dbus_connection_unref(connection);
}


static void nice_exit(int sig) {
	(void)sig;

	doneflag = 1;
	puts("\nClosing...\n");
}

static void daemon_handler(int sig) {
	(void)sig;
	/* Toggle spyflag so that it does not get recalled */
	if (spyflag == 0) {
		spyflag = 1;
		spy(NULL, LIVE_OUTPUT_OFF);
	} else {
		spyflag = 0;
		doneflag = 1;
	}

}


static void run_daemon() {
	/* Catch SIGUSR1. */
	struct sigaction act;
	act.sa_handler = daemon_handler;
	act.sa_flags = 0;

	if ((sigemptyset(&act.sa_mask) == -1) ||
		(sigaction(SIGUSR1, &act, NULL) == -1)) {
		perror("Failed to set SIGUSR1 handler");
		return;
	}

	#if DAHSEE_UI_WEB != 0
	run_server();
	#else
	for (;; ) {
		sleep(60);
	}
	#endif

	return;
}

/* TODO: replace this with freopen(): */
static void prepare_file(const char *path, FILE **file, const char *mode) {
	if (path == NULL) {
		*file = stdout;
	} else {
		if (strcmp(mode, "w") == 0) {
			/* Check if file exists, and if user allowed it to be overwritten. */
			if (option_force_overwrite == false && access(path, F_OK) == 0) {
				fprintf(logfile, "WARNING: File exists! Switching to standard output.\n");
				return;
			}
		}

		/* Note: binary mode is useless on POSIX. */
		*file = fopen(path, mode);
		if (*file == NULL) {
			perror(path);
			return;
		}
	}
}

/******************************************************************************/
/* Information */

static void print_help(const char *executable) {
	puts("A D-Bus monitoring tool.\n");
	printf("Usage: %s [FILTER]\n", executable);
	printf("   or: %s OPTION [ARG]\n\n", executable);

	puts("  -a        List activatable bus names.");
	#if DAHSEE_UI_WEB != 0
	printf("  -d        Daemonize on port %d.\n", PORT);
	#else
	puts("  -d        Daemonize.");
	#endif
	puts("  -f        Force overwriting when output file exists.");
	puts("  -h        Print this help.");
	puts("  -I NAME   Return introspection of NAME.");
	puts("  -L FILE   Write log to FILE (default is stderr).");
	puts("  -l        List registered bus names.");
	puts("  -n NAME   Return unique name associated to NAME.");
	puts("  -o FILE   Write output to FILE (default is stdout).");
	puts("  -p NAME   Return PID associated to NAME.");
	puts("  -u NAME   Return UID who owns NAME.");
	puts("  -v        Print version.");

	/* puts ("  -X        Set output format to XML."); */
	/* puts ("  -H        Set output format to HTML."); */
	/* puts ("  -i FILE   Read input FILE."); */

	puts("");
	puts("With no argument, it will catch D-Bus messages matching FILTER. The syntax follows D-Bus specification. If FILTER is empty, all messages are caught.");

	puts("");
	printf("See the %s(1) man page for more information.\n", APPNAME);
}

static void print_version() {
	printf("%s %s\n", APPNAME, VERSION);
	printf("Copyright © %s %s\n", YEAR, AUTHORS);
}

/******************************************************************************/
/* Main */

int main(int argc, char **argv) {
	/* getopt() variables. */
	int c;
	extern char *optarg;
	extern int optind;
	extern int optopt;
	const char *parameter = NULL;

	enum Queries query = QUERY_NONE;
	int exclusive_opt = 0;

	/* Set default log and output. */
	logfile = stderr;
	output = stdout;
	bool set_output = false;
	bool set_logfile = false;

	/* Catch SIGINT (Ctrl-C). */
	struct sigaction act;
	act.sa_handler = nice_exit;
	act.sa_flags = 0;

	if ((sigemptyset(&act.sa_mask) == -1) ||
		(sigaction(SIGINT, &act, NULL) == -1)) {
		perror("Failed to set SIGINT handler");
		return 1;
	}

	/* Fork variables. */
	pid_t pid, sid;
	bool daemonize = false;
	while ((c = getopt(argc, argv, ":adfhi:I:L:ln:o:p:vu:")) != -1) {
		switch (c) {
		case 'a':
			exclusive_opt++;
			query = QUERY_ACTIVATABLE_NAMES;
			break;

		case 'd':
			exclusive_opt++;
			daemonize = true;
			break;

		case 'f':
			option_force_overwrite = true;
			break;

		case 'h':
			print_help(argv[0]);
			return 0;

		case 'I':
			exclusive_opt++;
			parameter = optarg;
			query = QUERY_INTROSPECT;
			break;

		case 'L':
			logfile_path = optarg;
			set_logfile = true;
			break;

		case 'l':
			exclusive_opt++;
			query = QUERY_LIST_NAMES;
			break;

		case 'n':
			exclusive_opt++;
			parameter = optarg;
			query = QUERY_GET_NAME_OWNER;
			break;

		case 'o':
			output_path = optarg;
			set_output = true;
			break;

		case 'p':
			exclusive_opt++;
			parameter = optarg;
			query = QUERY_GET_CONNECTION_UNIX_PROCESS_ID;
			break;

		case 'u':
			exclusive_opt++;
			parameter = optarg;
			query = QUERY_GET_CONNECTION_UNIX_USER;
			break;

		case 'v':
			print_version();
			return 0;

		/* case 'X': */
		/*     option_output_format = FORMAT_XML; */
		/*     break; */

		/* case 'H': */
		/*     option_output_format = FORMAT_HTML; */
		/*     break; */

		/* case 'i': */
		/*     input_path = optarg; */
		/*     prepare_file(input_path, &input, "r"); */
		/*     break; */

		case ':':
			fprintf(logfile, "ERROR: -%c needs an argument.\n==> Try '%s -h' for more information.\n", optopt, argv[0]);
			return 1;
		case '?':
			fprintf(logfile, "ERROR: Unknown argument %c.\n==> Try '%s -h' for more information.\n", optopt, argv[0]);
			return 1;
		default:
			print_help(argv[0]);
			return 0;
		}
	}

	if (exclusive_opt > 1) {
		fprintf(logfile, "ERROR: Mutually exclusive arguments were given.\n==> Try '%s -h' for more information.\n", argv[0]);
		return 1;
	}

	if (set_logfile) {
		prepare_file(logfile_path, &logfile, "a");
	}

	if (set_output) {
		prepare_file(output_path, &output, "w");
	}


	/* TODO: check if useful. */
	/* Set stdout to be unbuffered; this is basically so that if people
	 * do dbus-monitor > file, then send SIGINT via Control-C, they
	 * don't lose the last chunk of messages.
	 */
	/* setvbuf (stdout, NULL, _IOLBF, 0); */

	/* TODO: combine import file with web and stats. */
	/* source_node = json_import(optarg); */
	/* json_print(source_node); */

	/* TODO: handle output formats. */
	/* source_node = json_import(optarg); */
	/* source_text=html_message(source_node); */
	/* printf("%s\n",source_text); */
	/* json_delete(source_node); */
	/* free(source_text); */
	/* return 0; */

	if (query != QUERY_NONE) {
		JsonNode *source_node;

		/* Queries */
		switch (query) {
		case QUERY_LIST_NAMES:
			json_print(query_bus(QUERY_LIST_NAMES, NULL));
			break;
		case QUERY_ACTIVATABLE_NAMES:
			json_print(query_bus(QUERY_ACTIVATABLE_NAMES, NULL));
			break;
		case QUERY_GET_NAME_OWNER:
			json_print(query_bus(QUERY_GET_NAME_OWNER, parameter));
			break;
		case QUERY_GET_CONNECTION_UNIX_USER:
			json_print(query_bus(QUERY_GET_CONNECTION_UNIX_USER, parameter));
			break;
		case QUERY_GET_CONNECTION_UNIX_PROCESS_ID:
			json_print(query_bus(QUERY_GET_CONNECTION_UNIX_PROCESS_ID, parameter));
			break;
		case QUERY_INTROSPECT:
			/* TODO: two possible result (use option): */

			/* XML-to-JSON. */
			source_node = json_find_member(
				json_find_element(
					json_find_member(
						query_bus(QUERY_INTROSPECT, parameter), DBUS_JSON_ARGS), 0), "value");

			json_print(dbus_xml_parser(source_node->string_));

			/* Raw XML. */
			/* source_node = json_find_member( */
			/*     json_find_element( */
			/*         json_find_member( */
			/*             query_bus (QUERY_INTROSPECT, parameter), DBUS_JSON_ARGS ),0), "value"); */

			/* if (source_node != NULL) */
			/*     fprintf (output, "%s\n", source_node->string_); */

			return 0;
		default:
			break;
		}
	}
	/* Daemon */
	else if (daemonize == true) {
		/* Fork off the parent process */
		pid = fork();
		if (pid < 0) {
			exit(EXIT_FAILURE);
		}
		if (pid > 0) {
			exit(EXIT_SUCCESS);
		}

		/* Change the file mode mask following parameters. */
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

		/* TODO: need to log somewhere, otherwise will go to terminal. */
		run_daemon();
	} else {
		/* Eavesdrop bus messages. Default bhaviour. */

		/* WARNING: considering that all remaining args are arguments may be */
		/* POSIXLY_INCORRECT.  TODO: check behaviour when POSIXLY_CORRECT is set. */

		char *filter;
		if (argc - optind != 1) {
			filter = NULL;
		} else {
			filter = argv[optind];
		}

		spy(filter, LIVE_OUTPUT_ON);

		/* TEST: */
		/* if (html_message != NULL) */
		/*     fprintf (stderr, "%s\n", html_message); */
	}


	/* Clean global stuff. */
	if (message_array != NULL) {
		json_delete(message_array);
	}

	if (html_message != NULL) {
		free(html_message);
	}

	if (output != NULL && output != stdout) {
		fclose(output);
	}

	if (logfile != NULL && logfile != stderr) {
		fclose(logfile);
	}

	if (input != NULL) {
		fclose(output);
	}

	return 0;
}
