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
Web interface with microhttpd.
Interface is optional, it comes as a convenient feature to display stats.
*/

#ifndef UI_WEB_H
#define UI_WEB_H 1

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <microhttpd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#define PORT 7117

void run_server();

#endif /* UI_WEB_H */
