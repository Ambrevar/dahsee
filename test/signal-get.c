#include "test.h"

/**
 * Listens for signals on the test bus.
 */
int 
main()
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    int ret;
    char* sigvalue;

    char filter[FILTER_SIZE];

    puts("Listening for signals");

    // Initialise the errors.
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
    ret = dbus_bus_request_name(conn, TEST_SERVICE, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err))
    { 
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err); 
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret)
    {
        fprintf(stderr, "Bus busy\n");
        return 1;
    }

    // Add a rule for which messages we want to see.
    // TODO: 
    snprintf(filter, FILTER_SIZE, "%s%s%s","type='signal',interface='", TEST_SIGNAL_INTERFACE, "'");

    dbus_bus_add_match(conn, filter, &err); // see signals from the given interface
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err))
    {
        fprintf(stderr, "Match Error (%s)\n", err.message);
        exit(1);
    }
    printf("Match rule sent.\n");
    printf("Filter: %s\n",filter);

    // loop listening for signals being emmitted
    while (true)
    {
        // Blocking (-1) read of the next available message
        dbus_connection_read_write_dispatch(conn, -1);
        msg = dbus_connection_pop_message(conn);

        // Check if the message is a signal from the correct interface and with the correct name.
        if (dbus_message_is_signal(msg, TEST_SIGNAL_INTERFACE, TEST_SIGNAL_NAME))
        {
            // Read the parameters.
            if (!dbus_message_iter_init(msg, &args))
                fprintf(stderr, "Message Has No Parameters\n");
            else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
                fprintf(stderr, "Argument is not string!\n"); 
            else
            {
                dbus_message_iter_get_basic(&args, &sigvalue);
         
                printf("Got [%s]\n", sigvalue);
            }
        }

        // free the message
        dbus_message_unref(msg);
    }

    return 0;
}
