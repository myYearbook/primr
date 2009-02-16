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

#ifndef TPOOL_H	                               /* Guarantee Single Inclusion */
#define TPOOL_H

/* ========================================================================= */
/* << INCLUSIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* Application-Specific Headers */
#include "primr.h"
#include "tpool.h"                                            /* Thread Pool */

/* ========================================================================= */
/* << TYPES / CONSTANTS / MACROS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/*
 * a generic thread pool creation routines
 */

typedef struct tpool_work
{
  void (*handler_routine)();
  void *arg;
  struct tpool_work *next;
} tpool_work_t;

typedef struct tpool
{
  int num_threads;
  int max_queue_size;

  int do_not_block_when_full;
  pthread_t *threads;
  int cur_queue_size;
  tpool_work_t *queue_head;
  tpool_work_t *queue_tail;
  pthread_mutex_t queue_lock;
  pthread_cond_t queue_not_full;
  pthread_cond_t queue_not_empty;
  pthread_cond_t queue_empty;
  int queue_closed;
  int shutdown;
} tpool_t;

/* ========================================================================= */
/* << GLOBAL VARIABLES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* ========================================================================= */
/* << FUNCTION PROTOTYPES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/*
 * returns a newly chreated thread pool
 */
extern tpool_t *tpool_init(int num_worker_threads,
    int max_queue_size, int do_not_block_when_full);

/*
 * returns -1 if work queue is busy
 * otherwise places it on queue for processing, returning 0
 *
 * the routine is a func. ptr to the routine which will handle the
 * work, arg is the arguments to that same routine
 */
extern int tpool_add_work(tpool_t *pool, void  (*routine)(), void *arg);

/*
 * cleanup and close,
 * if finish is set the any working threads will be allowd to finish
 */
extern int tpool_destroy(tpool_t *pool, bool finish);

#endif /* TPOOL_H */

/* vi: set et sw=4 ts=4: */

