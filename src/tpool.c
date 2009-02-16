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
**   tpool.c
**
** DESCRIPTION
**
**   Thread pool support code.
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
**   tpool_init         - Initializes the thread pool
**   tpool_add_work     - Adds work to the thread pool
**   tpool_destroy      - Destroys the thread pool
**
** PRIVATE FUNCTION(S)
**
**   tpool_thread       - Worker thread handler
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
**   SECURITY Verify everything related to memory is golden
**   LOW      Clean up usage() by removing multiple calls to printf()
**   LOW      Cleanup junk debug statements
**   LOW      Make sure all variable naming is consistent (lowerCamelCase)
**   LOW      Make DEBUG_PRINT multi-level (if needed)
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
**   15-FEB-2009 jharris  Clean-up for OSS release
**   13-OCT-2008 jharris  Original Implementation
** ============================================================================
*/
 
/* ========================================================================= */
/* << INCLUSIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

#include "primr.h"
#include "tpool.h"

/* ========================================================================= */
/* << TYPES / CONSTANTS / MACROS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* ========================================================================= */
/* << GLOBAL VARIABLES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* ========================================================================= */
/* << STATIC VARIABLES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* ========================================================================= */
/* << FUNCTION PROTOTYPES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

void *tpool_thread (void *tpool);

/* ========================================================================= */
/* << PUBLIC FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

tpool_t *
tpool_init (int num_worker_threads,
            int max_queue_size, int do_not_block_when_full)
{
    int i, rtn;
    tpool_t *pool;

    /* make the thread pool structure */
    if ((pool = (struct tpool *) malloc (sizeof (struct tpool))) == NULL)
    {
        ERROR_PRINT ("%s", "Unable to malloc() thread pool!\n");
        return NULL;
    }

    /* set the desired thread pool values */
    pool->num_threads = num_worker_threads;
    pool->max_queue_size = max_queue_size;
    pool->do_not_block_when_full = do_not_block_when_full;

    /* create an array to hold a ptr to the worker threads */
    if ((pool->threads = (pthread_t *) malloc (sizeof (pthread_t)
                                               * num_worker_threads)) == NULL)
    {
        ERROR_PRINT ("%s", "Unable to malloc() thread info array\n");
        return NULL;
    }

    /* initialize the work queue */
    pool->cur_queue_size = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_closed = 0;
    pool->shutdown = 0;

    /* create the mutexs and cond vars */
    if ((rtn = pthread_mutex_init (&(pool->queue_lock), NULL)) != 0)
    {
        ERROR_PRINT ("%s", "pthread_mutex_init %s", strerror (rtn));
        return NULL;
    }
    if ((rtn = pthread_cond_init (&(pool->queue_not_empty), NULL)) != 0)
    {
        ERROR_PRINT ("%s", "pthread_cond_init %s", strerror (rtn));
        return NULL;
    }
    if ((rtn = pthread_cond_init (&(pool->queue_not_full), NULL)) != 0)
    {
        ERROR_PRINT ("%s", "pthread_cond_init %s", strerror (rtn));
        return NULL;
    }
    if ((rtn = pthread_cond_init (&(pool->queue_empty), NULL)) != 0)
    {
        ERROR_PRINT ("%s", "pthread_cond_init %s", strerror (rtn));
        return NULL;
    }

    /* 
     * from "man 3c pthread_attr_init"
     * Define the scheduling contention scope for the created thread.  The only
     * value     supported    in    the    LinuxThreads    implementation    is
     * !PTHREAD_SCOPE_SYSTEM!, meaning that the threads contend  for  CPU  time
     * with all processes running on the machine.
     *
     * so no need to explicitly set the SCOPE
     */

    /* create the individual worker threads */
    for (i = 0; i != num_worker_threads; i++)
    {
        if ((rtn = pthread_create (&(pool->threads[i]), NULL,
                                   tpool_thread, (void *) pool)) != 0)
        {
            ERROR_PRINT ("%s", "pthread_create %s\n", strerror (rtn));
            return NULL;
        }
    }

    return pool;
}

/* ------------------------------------------------------------------------- */

int
tpool_add_work (tpool_t * pool, void (*routine) (), void *arg)
{
    int rtn;
    tpool_work_t *workp;

    if ((rtn = pthread_mutex_lock (&pool->queue_lock)) != 0)
    {
        ERROR_PRINT ("%s", "pthread mutex lock failure\n");
        return -1;
    }

    /* now we have exclusive access to the work queue ! */

    if ((pool->cur_queue_size == pool->max_queue_size) &&
        (pool->do_not_block_when_full))
    {
        if ((rtn = pthread_mutex_unlock (&pool->queue_lock)) != 0)
        {
            ERROR_PRINT ("%s", "pthread mutex lock failure\n");
            return -1;
        }
        return -1;
    }

    /* wait for the queue to have an open space for new work, while
     * waiting the queue_lock will be released */
    while ((pool->cur_queue_size == pool->max_queue_size) &&
           (!(pool->shutdown || pool->queue_closed)))
    {
        if ((rtn = pthread_cond_wait (&(pool->queue_not_full),
                                      &(pool->queue_lock))) != 0)
        {
            ERROR_PRINT ("%s", "pthread cond wait failure\n");
            return -1;
        }
    }

    if (pool->shutdown || pool->queue_closed)
    {
        if ((rtn = pthread_mutex_unlock (&pool->queue_lock)) != 0)
        {
            ERROR_PRINT ("%s", "pthread mutex lock failure\n");
            return -1;
        }
        return -1;
    }

    /* allocate the work structure */
    if ((workp = (tpool_work_t *) malloc (sizeof (tpool_work_t))) == NULL)
    {
        ERROR_PRINT ("%s", "unable to create work struct\n");
        return -1;
    }

    /* set the function/routine which will handle the work,
     * (note: it must be reenterant) */
    workp->handler_routine = routine;
    workp->arg = arg;
    workp->next = NULL;

    if (pool->cur_queue_size == 0)
    {
        pool->queue_tail = pool->queue_head = workp;
        if ((rtn = pthread_cond_broadcast (&(pool->queue_not_empty))) != 0)
        {
            ERROR_PRINT ("%s", "pthread broadcast error\n");
            return -1;
        }
    }
    else
    {
        pool->queue_tail->next = workp;
        pool->queue_tail = workp;
    }


    pool->cur_queue_size++;

    /* relinquish control of the queue */
    if ((rtn = pthread_mutex_unlock (&pool->queue_lock)) != 0)
    {
        ERROR_PRINT ("%s", "pthread mutex lock failure\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */

int
tpool_destroy (tpool_t * pool, bool finish)
{
    int i, rtn;
    tpool_work_t *cur;

    /* relinquish control of the queue */
    if ((rtn = pthread_mutex_lock (&(pool->queue_lock))) != 0)
    {
        ERROR_PRINT ("%s", "pthread mutex lock failure\n");
        return -1;
    }

    /* is a shutdown already going on ? */
    if (pool->queue_closed || pool->shutdown)
    {
        if ((rtn = pthread_mutex_unlock (&(pool->queue_lock))) != 0)
        {
            ERROR_PRINT ("%s", "pthread mutex lock failure\n");
            return -1;
        }
        return 0;
    }

    /* close the queue to any new work */
    pool->queue_closed = 1;

    /* if the finish flag is set, drain the queue */
    if (finish)
    {
        while (pool->cur_queue_size != 0)
        {
            /* wait for the queue to become empty,
             * while waiting queue lock will be released */
            if ((rtn = pthread_cond_wait (&(pool->queue_empty),
                                          &(pool->queue_lock))) != 0)
            {
                ERROR_PRINT ("%s", "pthread_cond_wait %d\n", rtn);
                return -1;
            }
        }
    }

    /* set the shutdown flag */
    pool->shutdown = 1;

    if ((rtn = pthread_mutex_unlock (&(pool->queue_lock))) != 0)
    {
        ERROR_PRINT ("%s", "pthread mutex unlock failure\n");
        return -1;
    }

    /* wake up all workers to rechedk the shutdown flag */
    if ((rtn = pthread_cond_broadcast (&(pool->queue_not_empty))) != 0)
    {
        ERROR_PRINT ("%s", "pthread_cond_boradcast %d\n", rtn);
        return -1;
    }
    if ((rtn = pthread_cond_broadcast (&(pool->queue_not_full))) != 0)
    {
        ERROR_PRINT ("%s", "pthread_cond_boradcast %d\n", rtn);
        return -1;
    }

    /* wait for workers to exit */
    for (i = 0; i < pool->num_threads; i++)
    {
        if ((rtn = pthread_join (pool->threads[i], NULL)) != 0)
        {
            ERROR_PRINT ("%s", "pthread_join %d\n", rtn);
            return -1;
        }
    }

    /* clean up memory */
    free (pool->threads);
    while (pool->queue_head != NULL)
    {
        cur = pool->queue_head->next;
        pool->queue_head = pool->queue_head->next;
        free (cur);
    }
    free (pool);

    return 0;
}

/* ========================================================================= */
/* << PRIVATE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

void *
tpool_thread (void *tpool)
{
    tpool_work_t *my_work;
    tpool_t *pool = (struct tpool *) tpool;
    int *soket;

    for (;;)                    /* go forever */
    {

        pthread_mutex_lock (&(pool->queue_lock));


        /* sleep until there is work,
         * while asleep the queue_lock is relinquished */
        while ((pool->cur_queue_size == 0) && (!pool->shutdown))
        {
            pthread_cond_wait (&(pool->queue_not_empty), &(pool->queue_lock));
        }

        /* are we shutting down ? */
        if (pool->shutdown)
        {
            pthread_mutex_unlock (&(pool->queue_lock));
            pthread_exit (NULL);
        }

        /* process the work */
        my_work = pool->queue_head;
        pool->cur_queue_size--;
        if (pool->cur_queue_size == 0)
            pool->queue_head = pool->queue_tail = NULL;
        else
            pool->queue_head = my_work->next;


        /* broadcast that the queue is not full */
        if ((!pool->do_not_block_when_full) &&
            (pool->cur_queue_size == (pool->max_queue_size - 1)))
        {
            pthread_cond_broadcast (&(pool->queue_not_full));
        }

        if (pool->cur_queue_size == 0)
        {
            pthread_cond_signal (&(pool->queue_empty));
        }

        pthread_mutex_unlock (&(pool->queue_lock));

        soket = (int *) my_work->arg;
        /* perform the work */
        (*(my_work->handler_routine)) (my_work->arg);
        free (my_work);
    }
    return (NULL);
}

/* vi: set et sw=4 ts=4: */

