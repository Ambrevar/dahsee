#include "test.h"

/**
 * List names on "session bus".
 */
int
main()
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
    
    return 0;
}

