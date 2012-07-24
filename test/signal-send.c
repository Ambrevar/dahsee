#include "test.h"

/**
 * Connect to the DBUS bus and send a broadcast signal
 */
int
main(int argc, char** argv)
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    int ret;
    dbus_uint32_t serial = 0;

    // NOTE: 
    // dbus_message_iter_append_basic() will crash when last argument is a char[] instead of a char*.
    char* sigvalue;

    if (argc == 1)
    {
        sigvalue=SIGNAL_DEFAULT_VALUE;
    }
    else
    {
        sigvalue=argv[1];
    }

    printf("Sending signal with value [%s]\n", sigvalue);

    // initialise the error value
    dbus_error_init(&err);

    // connect to the DBUS system bus, and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Connection Error (%s)\n", err.message); 
        dbus_error_free(&err); 
    }
    if (NULL == conn) { 
        exit(1); 
    }

    // register our name on the bus, and check for errors
    ret = dbus_bus_request_name(conn, TEST_SERVICE, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Name Error (%s)\n", err.message); 
        dbus_error_free(&err); 
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
        exit(1);
    }

    // create a signal & check for errors 
    msg = dbus_message_new_signal(TEST_SIGNAL_OBJECT, // object name of the signal
                                  TEST_SIGNAL_INTERFACE, // interface name of the signal
                                  TEST_SIGNAL_NAME); // name of the signal
    if (NULL == msg) 
    { 
        fprintf(stderr, "Message Null\n"); 
        exit(1); 
    }

    // append arguments onto signal
    dbus_message_iter_init_append(msg, &args);

    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &sigvalue)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }

    // send the message and flush the connection
    if (!dbus_connection_send(conn, msg, &serial)) {
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    dbus_connection_flush(conn);
   
    printf("Signal Sent, serial=%u\n",serial);
   
    // free the message and close the connection
    dbus_message_unref(msg);
    dbus_connection_unref(conn);

    return 0;
}
