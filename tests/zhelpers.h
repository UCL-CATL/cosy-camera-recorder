/*  =====================================================================
    zhelpers.h

    Helper header file for example applications.
    =====================================================================
*/

#ifndef __ZHELPERS_H_INCLUDED__
#define __ZHELPERS_H_INCLUDED__

//  Include a bunch of headers that we will need in the examples

#include <zmq.h>

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if (!defined (WIN32))
#   include <sys/time.h>
#endif

#if (defined (WIN32))
#   include <windows.h>
#endif

//  Version checking, and patch up missing constants to match 2.1
#if ZMQ_VERSION_MAJOR == 2
#   error "Please upgrade to ZeroMQ/3.2 for these examples"
#endif

//  On some version of Windows, POSIX subsystem is not installed by default.
//  So define srandom and random ourself.
#if (defined (WIN32))
#   define srandom srand
#   define random rand
#endif

//  Provide random number from 0..(num-1)
#define randof(num)  (int) ((float) (num) * random () / (RAND_MAX + 1.0))

//  Receive 0MQ string from socket and convert into C string
//  Caller must free returned string. Returns NULL if the context
//  is being terminated.
static char *
s_recv (void *socket) {
    char buffer [256];
    int size = zmq_recv (socket, buffer, 255, 0);
    if (size == -1)
        return NULL;
    if (size > 255)
        size = 255;
    buffer [size] = 0;
    return strdup (buffer);
}

#endif  //  __ZHELPERS_H_INCLUDED__
