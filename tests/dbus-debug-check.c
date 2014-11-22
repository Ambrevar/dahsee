#include "test.h"

/**
 * Debug: check Dbus debugging functionalities.
 */
int main()
{
    DBusConnection* conn;
    DBusError err;

    printf("Debug\n");

    // Initialise the errors.
    // If this line is commented, Dbus connection will fail to initialize and crash.
    /* dbus_error_init(&err); */
   
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
    dbus_bus_request_name(conn, TEST_SERVICE, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err); 
    }

    return 0;
}
