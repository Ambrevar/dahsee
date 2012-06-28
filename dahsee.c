/*******************************************************************************
 * @file dahsee.c
 * @date 2012-06-28
 * @brief Dbus monitoring tool
 ******************************************************************************/

// TODO: test dbus_connection_open on existing connection.
// Cf. dbus-monitor.


#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FILTER_SIZE 1024

#define SPY_BUS "spy.lair"

#define TEST_SIGNAL_BUS "test.signal.sink"
#define TEST_SIGNAL_INTERFACE "test.signal.Type"
#define TEST_SIGNAL_OBJECT "/test/signal/Object"
#define TEST_SIGNAL_NAME "Whatevername"


/**
 * Spy: catch signals.
 */
void spy(/* char* param */)
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    char* sigvalue;

    char filter[1024];

    printf("Listening for signals\n");

    // initialise the errors
    dbus_error_init(&err);
   
    // connect to the bus and check for errors
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
    snprintf(filter, FILTER_SIZE, "%s","type='signal'");

    dbus_bus_add_match(conn, filter, &err); // see signals from the given interface
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Match Error (%s)\n", err.message);
        exit(1);
    }
    printf("Match rule sent.\n");
    printf("Filter: %s\n",filter);

    // loop listening for signals being emmitted
    while (true) {

        // non blocking read of the next available message
        dbus_connection_read_write_dispatch(conn, -1);
        msg = dbus_connection_pop_message(conn);

        if(msg != NULL)
        {
            // check if the message is a signal from the correct interface and with the correct name
            /* if (dbus_message_is_signal(msg, TEST_SIGNAL_INTERFACE, TEST_SIGNAL_NAME)) */
            /* { */
            // read the parameters
            if (!dbus_message_iter_init(msg, &args))
                fprintf(stderr, "Message Has No Parameters\n");
            else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
                fprintf(stderr, "Argument is not string!\n"); 
            else
            {
                dbus_message_iter_get_basic(&args, &sigvalue);
         
                printf("Got Signal with value %s\n", sigvalue);
            }
            /* } */
      
            // free the message
            dbus_message_unref(msg);
        }
    }
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
 * Connect to the DBUS bus and send a broadcast signal
 */
void sendsignal(char* sigvalue)
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    int ret;
    dbus_uint32_t serial = 0;

    printf("Sending signal with value %s\n", sigvalue);

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
    ret = dbus_bus_request_name(conn, "test.signal.source", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
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
}

/**
 * Call a method on a remote object
 */
void query(char* param) 
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    int ret;
    bool stat;
    dbus_uint32_t level;

    printf("Calling remote method with %s\n", param);

    // initialiset the errors
    dbus_error_init(&err);

    // connect to the system bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Connection Error (%s)\n", err.message); 
        dbus_error_free(&err);
    }
    if (NULL == conn) { 
        exit(1); 
    }

    // request our name on the bus
    ret = dbus_bus_request_name(conn, "test.method.caller", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Name Error (%s)\n", err.message); 
        dbus_error_free(&err);
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
        exit(1);
    }

    // create a new method call and check for errors
    msg = dbus_message_new_method_call("test.method.servers", // target for the method call
                                       "/test/method/Object", // object to call on
                                       "test.method.Type", // interface to call on
                                       "Method"); // method name
    if (NULL == msg) { 
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

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
    else if (DBUS_TYPE_BOOLEAN != dbus_message_iter_get_arg_type(&args))
        fprintf(stderr, "Argument is not boolean!\n");
    else
        dbus_message_iter_get_basic(&args, &stat);

    if (!dbus_message_iter_next(&args))
        fprintf(stderr, "Message has too few arguments!\n");
    else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args))
        fprintf(stderr, "Argument is not int!\n");
    else
        dbus_message_iter_get_basic(&args, &level);

    printf("Got Reply: %d, %d\n", stat, level);
   
    // free reply and close connection
    dbus_message_unref(msg);   
    /* dbus_connection_close(conn); */
}

void reply_to_method_call(DBusMessage* msg, DBusConnection* conn)
{
    DBusMessage* reply;
    DBusMessageIter args;
    dbus_bool_t stat = true;
    dbus_uint32_t level = 21614;
    dbus_uint32_t serial = 0;
    char* param = "";

    // read the arguments
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n"); 
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
        fprintf(stderr, "Argument is not string!\n"); 
    else 
        dbus_message_iter_get_basic(&args, &param);

    printf("Method called with %s\n", param);

    // create a reply from the message
    reply = dbus_message_new_method_return(msg);

    if (reply == NULL) 
    {
        fprintf(stderr,"Cannot allocate reply!\n");
    }


    // add the arguments to the reply
    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &stat)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &level)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }

    // send the reply && flush the connection
    if (!dbus_connection_send(conn, reply, &serial)) {
        fprintf(stderr, "Out Of Memory!\n"); 
        exit(1);
    }
    dbus_connection_flush(conn);

    // free the reply
    dbus_message_unref(reply);
}

/**
 * Server that exposes a method call and waits for it to be called
 */
void
listen() 
{
    DBusMessage* msg;
    /* DBusMessage* reply; */
    /* DBusMessageIter args; */
    DBusConnection* conn;
    DBusError err;
    int ret;
    /* char* param; */

    printf("Listening for method calls\n");

    // initialise the error
    dbus_error_init(&err);

   
    // connect to the bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Connection Error (%s)\n", err.message); 
        dbus_error_free(&err); 
    }
    if (NULL == conn) {
        fprintf(stderr, "Connection Null\n"); 
        exit(1); 
    }

   
    // request our name on the bus and check for errors
    ret = dbus_bus_request_name(conn, "test.method.servers", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Name Error (%s)\n", err.message); 
        dbus_error_free(&err);
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
        fprintf(stderr, "Not Primary Owner (%d)\n", ret);
        exit(1); 
    }

    // loop, testing for new messages
    while (true) {
        // non blocking read of the next available message
        /* dbus_connection_read_write_dispatch(conn, -1); */

        dbus_connection_read_write(conn,0);
        msg = dbus_connection_pop_message(conn);

        // loop again if we haven't got a message
        if (NULL == msg) {
            sleep(1);
            continue;
        }
      
        // check this is a method call for the right interface & method
        if (dbus_message_is_method_call(msg, "test.method.Type", "Method")) 
        {
            reply_to_method_call(msg, conn);
        }

        // free the message
        dbus_message_unref(msg);
    }

    printf("Looping4\n");

    // close the connection
    /* dbus_connection_close(conn); */
}

/**
 * Listens for signals on the bus
 */
void
receive()
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    int ret;
    char* sigvalue;

    char filter[1024];

    printf("Listening for signals\n");

    // initialise the errors
    dbus_error_init(&err);
   
    // connect to the bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if ( dbus_error_is_set ( &err ) ) { 
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err); 
    }
    if (NULL == conn) { 
        exit(1);
    }
   
    // request our name on the bus and check for errors
    ret = dbus_bus_request_name(conn, TEST_SIGNAL_BUS, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err); 
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
        fprintf(stderr, "Bus busy\n");
        exit(1);
    }

    // add a rule for which messages we want to see
   
   
    snprintf(filter, FILTER_SIZE, "%s%s%s","type='signal',interface='", TEST_SIGNAL_INTERFACE, "'");

    dbus_bus_add_match(conn, filter, &err); // see signals from the given interface
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Match Error (%s)\n", err.message);
        exit(1);
    }
    printf("Match rule sent.\n");
    printf("Filter: %s\n",filter);

    // loop listening for signals being emmitted
    while (true) {

        // non blocking read of the next available message
        dbus_connection_read_write_dispatch(conn, -1);
        msg = dbus_connection_pop_message(conn);

        /* loop again if we haven't read a message */
        /* if (NULL == msg) { */
        /*    sleep(1); */
        /*    continue; */
        /* } */

        // check if the message is a signal from the correct interface and with the correct name
        if (dbus_message_is_signal(msg, TEST_SIGNAL_INTERFACE, TEST_SIGNAL_NAME))
        {
            // read the parameters
            if (!dbus_message_iter_init(msg, &args))
                fprintf(stderr, "Message Has No Parameters\n");
            else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
                fprintf(stderr, "Argument is not string!\n"); 
            else
            {
                dbus_message_iter_get_basic(&args, &sigvalue);
         
                printf("Got Signal with value %s\n", sigvalue);
            }
        }

        // free the message
        dbus_message_unref(msg);
    }
}

inline void 
help(const char* name)
{
    printf ("Syntax: %s [send|receive|listen|query|list|spy] [<param>]\n", name);
}

int main(int argc, char** argv)
{
    char* param = "no param";

    if (2 > argc) {
        help(argv[0]);
        return 1;
    }

    if (argc >= 3 && NULL != argv[2]) param = argv[2];
    if (0 == strcmp(argv[1], "send"))
        sendsignal(param);
    else if (0 == strcmp(argv[1], "receive"))
        receive();
    else if (0 == strcmp(argv[1], "listen"))
        listen();
    else if (0 == strcmp(argv[1], "query"))
        query(param);
    else if (0 == strcmp(argv[1], "list"))
        list();
    else if (0 == strcmp(argv[1], "spy"))
        spy(param);
    else {
        help(argv[0]);
        return 1;
    }
    return 0;
}
