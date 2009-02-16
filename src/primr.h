/* $Id$
** ============================================================================
**                                  P R I M R
**                  AN OPERATING SYSTEM CACHE PRIMER UTILITY
**
**      Copyright (c) 2008,2009 Insider Guides, Inc. All Rights Reserved.
** ============================================================================
**
** FILENAME
**
**   primr.h
**
** DESCRIPTION
**
**   Primer definitions and required header files.
**
** INSPECTION STATUS 
**
**   Inspection date..............
**   Inspection status............
**   Estimated defects per page...
**   Rule sets....................
** 
** ACCEPTANCE REVIEW STATUS 
**
**   Review date..................
**   Review status................
**   Reviewers....................
**
** PUBLIC FUNCTION(S) 
**
**   N/A
**
** PRIVATE FUNCTION(S)
**
**   N/A
**
** NOTES
**
**   + This has been tested with PostgreSQL 8.2/8.3
**
** USAGE
**
**   $ primr --help
**
** COPYRIGHT ACKNOWLEDGEMENTS
**
**   Copyright (c) 2008,2009 Insider Guides, Inc. All Rights Reserved.
**
** ORIGINAL AUTHORS
**
**   Jonah H. Harris <jonah.harris@myyearbook.com>
**
** CONTRIBUTING AUTHORS
**
**   N/A
**
** MAINTAINERS
**
**   myYearbook Development Team <developers@myyearbook.com>
**
** REFERENCE MATERIAL
**
**   http://www.postgresql.org/
**      - PostgreSQL Manual (8.x)
**
**   http://www.google.com/
**      - Everything Else
**
** AKNOWLEDGEMENTS
**
**   Juan Valdez and his faithful burro Lana for the much-needed coffee.
**
** TASK AGENDA
**
**   PRIORITY TASK
**   -------- -----------------------------------------------------------------
**   LOW      Make sure all code wraps at col 80
**   ======== =================================================================
**
** SOFTWARE LICENSE
**
**   Copyright (c) 2008,2009 Insider Guides, Inc. 
**   All Rights Reserved.
**
**   Redistribution and use in source and binary forms, with or without
**   modification, are permitted provided that the following conditions are
**   met:
**
**      * Redistributions of source code must retain the above copyright
**        notice, this list of conditions and the following disclaimer.
**
**      * Redistributions in binary form must reproduce the above copyright
**        notice, this list of conditions and the following disclaimer in the
**        documentation and/or other materials provided with the distribution.
**
**      * Neither the name of the Insider Guides, Inc. nor the names of its
**        contributors may be used to endorse or promote products derived
**        from this software without specific prior written permission.
**
**   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
**   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
**   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
**   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
**   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
**   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
**   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
**   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** REVISION HISTORY
**
**       DATE      WHO    DESCRIPTION
**   ----------- -------- -----------------------------------------------------
**   16-FEB-2009 jharris  Clean-up for OSS release
**   13-OCT-2008 jharris  Original Implementation
** ============================================================================
*/

#ifndef PRIMR_H                                /* Guarantee Single Inclusion */
#define PRIMR_H

/* ========================================================================= */
/* << INCLUSIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* Standard C Headers */
#include <stdio.h>                                /* Standard C Input/Output */
#include <stdlib.h>                                    /* Standard C Library */
#include <stdbool.h>                             /* Standard Boolean Library */
#include <stdint.h>                              /* Standard Integer Library */
#include <string.h>                               /* Standard String Library */
#include <fcntl.h>                                           /* File Control */
#include <dirent.h>                                 /* POSIX File Operations */
#include <getopt.h>                                  /* Command-line Parsing */
#include <errno.h>                                /* Standard Error Handling */
#include <unistd.h>                           /* Standard Symbolic Constants */
#include <sys/mman.h>                                   /* Memory Management */
#include <sys/stat.h>                               /* Needed for stat/fstat */
#include <sys/types.h>                                  /* Needed for size_t */
#include <fnmatch.h>                 /* Needed for Filename Pattern Matching */

/* Application-Specific Headers */
#include "tpool.h"                                            /* Thread Pool */

/* Database Headers */
#include "sqlite3.h"                             /* Embedded SQLite Database */
#include "libpq-fe.h"                                     /* Postgres Client */

#endif

/* ========================================================================= */
/* << TYPES / CONSTANTS / MACROS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* ---------------------------- Useful Macros ------------------------------ */

/* void DEBUG_PRINT(char *fmt, ...); */
#define DEBUG_PRINT(fmt, ...)                                                 \
    {                                                                         \
        if (is_debug)                                                         \
        {                                                                     \
            fprintf(stderr, "[%s/%d]: DEBUG: ",                               \
                __PRETTY_FUNCTION__, __LINE__);                               \
            fprintf(stderr, fmt, __VA_ARGS__);                                \
            fflush(stderr);                                                   \
        }                                                                     \
    }
    /*
     * If debugging has been enabled, print debug statements to stderr.
     */

/* void ERROR_PRINT(char *fmt, ...); */
#define ERROR_PRINT(fmt, ...)                                                 \
    {                                                                         \
        fprintf(stderr, "[%s/%d]: ERROR: ",                                   \
                __PRETTY_FUNCTION__, __LINE__);                               \
        fprintf(stderr, fmt, __VA_ARGS__);                                    \
        fflush(stderr);                                                       \
    }
    /*
     * If debugging has been enabled, print debug statements to stderr.
     */

/* word POINTER_IS_VALID(void *ptr); */
#define POINTER_IS_VALID(ptr) ((void *)(ptr) != NULL)
    /*
     * Determine if the pointer ptr is valid.  This macro is used simply to
     * verify that the referenced pointer is not equal to NULL.
     */

/* vi: set et sw=4 ts=4: */


