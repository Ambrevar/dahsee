# Dahsee - A D-Bus monnitoring tool
### By Pierre Neidhardt


Description
-----------

D-Bus development can be quiet painful because of the lack of analysis and
monitoring tools.

The dbus-monitor, while light and fast, may lack some features to you.
Dahsee aims to be full featured while being lightweight and simple.

* Built in C.
* There is no dependency but D-Bus and a C library.
* The source code for the main program is a single C file.
* The program is only one lightwieght executable.

Features:

* D-Bus messages filtering.
* Comprehensive message description: timestamp, arguments, data payload, etc.
* Statistics over scanned massages.
* Comprehensive details about applications registered on the bus.
* The configurable and clear output to terminal can be easily parsed and piped
  to other application (as well as to itself).

Configuration
-------------

The only required dependency is D-Bus. You will need it both for compile-time
and runtime. It should compile and run on any POSIX system where D-Bus can run.

Optionally you will need microhttpd (a.k.a. libmicrohttpd) for the Web
interface.

You can adapt the compilation and installation options in `config.mk`.

Installation
------------

Optional: run `make ui` to embed the web interface in the build.

Run `make install` to install the program to the /usr/local prefix. You can
change the prefix from command-line, e.g.

	make install prefix='/usr'

Packagers may want to install the program to a specific folder:

	make install DESTDIR='<destdir>'

Project status
--------------

This project has been unmaintained since August 2012. Feel free to take it over.
