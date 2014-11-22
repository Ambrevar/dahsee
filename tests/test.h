#include <dbus/dbus.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SIGNAL_SIZE 1024
#define SIGNAL_DEFAULT_VALUE "This is a test signal."
#define METHOD_DEFAULT_VALUE "This is a method call parameter."

#define TEST_SERVICE "test.service"

#define TEST_SIGNAL_INTERFACE "test.signal.Type"
#define TEST_SIGNAL_OBJECT "/test/signal/Object"
#define TEST_SIGNAL_NAME "TestSignalName"

#define TEST_METHOD_SERVICE "test.method.service"
#define TEST_METHOD_INTERFACE "test.method.Type"
#define TEST_METHOD_OBJECT "/test/method/Object"
#define TEST_METHOD_NAME "TestMethodName"

#define FILTER_SIZE 1024

