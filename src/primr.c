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
**   primr.c
**
** DESCRIPTION
**
**   The main primr application code.
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
**   main - The program accessor
**
** PRIVATE FUNCTION(S)
**
**   BuildCacheProfile      - Profiles the cache
**   PrimeCache             - Primes the cache
**   ProfileCacheForFile    - Profiles the cache of a given file
**   PrimeCacheForFile      - Primes the cache for a given file
**   usage                  - Display primr usage information
**   xstrdup                - Error-checking strdup()
**   busyHandler            - SQLite SQLITE_BUSY handler callback
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
**   LOW      Optimize memory allocation in reloadConfig
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

/* Required Headers */
#include "primr.h"                                          /* Primr Headers */

/* ========================================================================= */
/* << TYPES / CONSTANTS / MACROS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* -------------------------- Package Definitions -------------------------- */

#define APP_RELEASE     "Alpha"
#define APP_COPYRIGHT   \
    "Copyright (c) 2008,2009 Insider Guides, Inc. All Rights Reserved."

/* ------------------------------------------------------------------------- */

/* ---------------------------- SQLite Queries ----------------------------- */

/*
 * When deciding when to fork backends, we need to consider the time when
 * each was started on the capture system.  To that effect, we're going to
 * query them from the capture in ascending order of their start time.  This
 * data is then used to perform time-based client process creation.
 */
#define BACKEND_QUERY                                                         \
    "SELECT backend_pid, backend_start_tms"                                   \
    "  FROM backends"                                                         \
    " ORDER BY backend_start_tms, backend_pid ASC"

/*
 * After a particular backend process is forked, the statements executed
 * for that backend are then queried in order of execution.  Replay is then
 * performed on the resulting data set.
 */
#define STATEMENT_QUERY                                                       \
    "SELECT backend_stmt_id,"                                                 \
    "       stmt_exec_tms,"                                                   \
    "       stmt_type,"                                                       \
    "       stmt_name,"                                                       \
    "       stmt_params,"                                                     \
    "       stmt_text"                                                        \
    "  FROM backend_queries"                                                  \
    " WHERE backend_pid = %u"                                                 \
    " ORDER BY backend_stmt_id ASC"


/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_ADD_TABLESPACE                                                    \
    "INSERT INTO tablespaces"                                                 \
    "       (oid, name, path) "                                               \
    "VALUES (?, ?, ?)"

/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_ADD_CACHED_FILE                                                   \
    "INSERT INTO cached_files"                                                \
    "       (file_name) "                                                     \
    "VALUES (%Q)"

/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_FILE_PAGE_TO_CACHE                                                \
    "INSERT INTO page_cache"                                                  \
    "       (file_id, page_number) "                                          \
    "VALUES (?, ?)"

/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_DELETE_CACHED_FILE                                                \
    "DELETE FROM cached_files"                                                \
    " WHERE file_id = %d"

/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_CACHED_FILES_QUERY                                                \
    "SELECT file_id"                                                          \
    "  FROM cached_files"

/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_CACHED_FILE_QUERY                                                 \
    "SELECT file_name"                                                        \
    "  FROM cached_files"                                                     \
    " WHERE file_id = ?"

/*
 * This query retrieves the file id associated with the file which was last
 * inserted into cached_files.
 */
#define SQL_LAST_INSERTED_FILE_ID_QUERY                                       \
    "SELECT file_id"                                                          \
    "  FROM cached_files"                                                     \
    " WHERE rowid = last_insert_rowid()"

/*
 * When deciding when to fork backends, we need to consider the time when
 * each was started on the capture system.  To that effect, we're going to
 * query them from the capture in ascending order of their start time.  This
 * data is then used to perform time-based client process creation.
 */
#define SQL_FILE_PATH_QUERY                                                   \
    "SELECT backend_pid, backend_start_tms"                                   \
    "  FROM backends"                                                         \
    " ORDER BY backend_start_tms, backend_pid ASC"

/*
 * While we store all pages found in the Linux Page Cache for database objects
 * of interest, for performance reasons, we don't want to read them all
 * off-disk in a one-by-one fashion.  As such, when we retrieve the list of
 * pages from SQLite for cache priming purposes, we group all sequential pages
 * into start/end ranges so that we can easily perform multi-block reads and
 * take advantage of sequential I/O where possible.
 */
#define SQL_PAGE_CACHE_RANGE_QUERY                                            \
    "SELECT t1.page_number AS start_block,"                                   \
    "       MIN(t2.page_number) AS end_block"                                 \
    "  FROM (SELECT page_number"                                              \
    "          FROM page_cache tbl1"                                          \
    "         WHERE file_id = %d"                                             \
    "               AND NOT EXISTS (SELECT page_number"                       \
    "                                 FROM page_cache tbl2"                   \
    "                                WHERE file_id = %d"                      \
    "                                      AND tbl1.page_number"              \
    "                                          = tbl2.page_number + 1)"       \
    "       ) t1"                                                             \
    " INNER JOIN (SELECT page_number"                                         \
    "               FROM page_cache tbl1"                                     \
    "              WHERE file_id = %d"                                        \
    "                    AND NOT EXISTS (SELECT page_number"                  \
    "                                      FROM page_cache tbl2"              \
    "                                     WHERE file_id = %d"                 \
    "                                           AND tbl2.page_number"         \
    "                                               = tbl1.page_number + 1)"  \
    "            ) t2"                                                        \
    "         ON t1.page_number <= t2.page_number"                            \
    " GROUP BY t1.page_number"

/* ------------------------------------------------------------------------- */

/* --------------------------- Postgres Queries ---------------------------- */

/*
 * This query acquires the list of objects we'll be caching from the database.
 * Because our cache primer only cares about table and index objects, we
 * perform selection on those object types.  The rest are either irrelevant
 * or require an insignificant amount of time to retrieve from disk.
 */
#define PG_OBJECT_QUERY                                                       \
    "SELECT relname,"                                                         \
    "       relkind,"                                                         \
    "       reltablespace,"                                                   \
    "       relfilenode"                                                      \
    "  FROM pg_class"                                                         \
    " WHERE relnamespace <> 11"                                               \
    "       AND relkind IN ('t', 'r', 'i')"                                   \
    "       AND (reltablespace = 0 OR reltablespace > 10000)"                 \
    " ORDER BY reltablespace, relname;"

/*
 * This query acquires the list of file paths used by the database.
 */
#define PG_TABLESPACE_QUERY                                                   \
    "SELECT oid,"                                                             \
    "       spcname,"                                                         \
    "       spclocation"                                                      \
    "  FROM pg_tablespace;"

/*
 * This query acquires the default file path used by the database.
 */
#define PG_DEFAULT_TABLESPACE_QUERY                                           \
    "SELECT oid,"                                                             \
    "       dattablespace"                                                    \
    "  FROM pg_database"                                                      \
    " WHERE datname = current_database();"

/*
 * This query acquires the block sized used by the given PG database.
 */
#define PG_BLCKSZ_QUERY                                                       \
    "SELECT setting"                                                          \
    "  FROM pg_settings"                                                      \
    " WHERE name = 'block_size';"

/* ========================================================================= */
/* << GLOBAL VARIABLES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

/* ========================================================================= */
/* << STATIC VARIABLES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

static bool         is_debug = false;               /* Is debugging enabled? */
static uint32_t     db_block_size = 8192;             /* Postgres Block Size */
static char        *pgConnectString = NULL;       /* Postgres Connect String */
static char        *pgPassword = NULL;        /* Postgres Password (if reqd) */
static char        *pgDataDir = NULL;                     /* Path for PGDATA */
static char        *dbFileName = NULL;         /*  SQLite database file name */
static tpool_t     *thp = NULL;                               /* Thread Pool */

/* ========================================================================= */
/* << FUNCTION PROTOTYPES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

void BuildCacheProfile (void);
void PrimeCache (void);
void ProfileCacheForFile (char *filename);
void PrimeCacheForFile (void *fileNumber);
static void usage (void);
static char *xstrdup (const char *s);

/* ========================================================================= */
/* << PUBLIC FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

int
main (int argc, char **argv)
{
    int             opt = 0;                            /* Option Identifier */
    int             optindex = 0;                            /* Option Index */
    bool            isProfiling = false;      /* Are we profiling the cache? */
    bool            isPriming = false;          /* Are we priming the cache? */
    long            numCPUs = 0;                /* The number of online CPUs */
    struct dirent  *dp = NULL;                    /* Directory Entry Pointer */
    DIR            *dfd = NULL;                          /* Directory Stream */
    double          loadAverages[3] = { 0.00 };      /* System Load Averages */
    PGconn         *pgh = NULL;                /* Postgres Connection Handle */
    bool            isPWDRequired = false;     /* Is Postgres Password Reqd? */

    struct option   long_options[] =                 /* Options for getopt() */
    {
        {"connect-string",  required_argument,  NULL, 'c'},
        {"profile",         no_argument,        NULL, 'p'},
        {"prime",           no_argument,        NULL, 'w'},
        {"data-dir",        required_argument,  NULL, 'D'},
        {"postgres-only",   no_argument,        NULL, 'o'},
        {"sqlite-db",       required_argument,  NULL, 's'},
        {"help",            no_argument,        NULL, 'h'},
        {"debug",           no_argument,        NULL, 'd'},
        {NULL, 0, NULL, 0}
    };

    /* Go for the glory! */
    fprintf(stderr, "\n%s: Release %s - %s\n", PACKAGE_NAME,
        PACKAGE_VERSION, APP_RELEASE);
    fprintf(stderr, "\n%s\n\n", APP_COPYRIGHT);
    fflush(stdout);

    /* Process command-line options */
    while ((opt = getopt_long(argc, argv, "c:s:D:awhdp",
        long_options, &optindex)) != -1)
    {
        switch (opt)
        {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'p':
                if (isPriming == false)
                    isProfiling = true;
                else
                {
                    fprintf(stderr,
                        "Profiling and warming are mutually exlusive!\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'w':
                if (isProfiling == false)
                    isPriming = true;
                else
                {
                    fprintf(stderr,
                        "Profiling and warming are mutually exlusive!\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                is_debug = true;
                break;
            case 's':
                dbFileName = xstrdup(optarg);
                break;
            case 'c':
                pgConnectString = xstrdup(optarg);
                break;
            case 'D':
                pgDataDir = optarg;
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    /* Make sure user requested profile OR prime */
    if (isProfiling == false && isPriming == false)
    {
        fprintf(stderr, "Expected either -p or -w\n");
        usage();
        exit(EXIT_FAILURE);
    }

    /* Make sure the database name is set */

    /* Get the PG log file name from the end of the command line */
    if (optind < (argc - 1))
    {
        fprintf(stderr, "too many command-line arguments (first is \"%s\")\n",
                argv[optind + 1]);
        usage();
        exit(EXIT_FAILURE);
    }

    /* Perform a Postgres connection test & get password (if required) */
    do
    {
        isPWDRequired = false;
        pgh = PQsetdbLogin(NULL, NULL, NULL, NULL,
            pgConnectString == NULL ? "postgres"
                                    : pgConnectString,
            NULL, pgPassword);
        if (PQstatus(pgh) == CONNECTION_BAD)
        {
            if (PQconnectionNeedsPassword(pgh) && pgPassword == NULL)
            {
                printf("\nTesting Postgres Connection\n");
                PQfinish(pgh);
                pgPassword = simple_prompt("Password: ", 100, false);
                isPWDRequired = true;
            }
            else
            {
                ERROR_PRINT("%s", "Connection Test Failed\n");
                ERROR_PRINT("SQLERRMC: %s\n", PQerrorMessage(pgh));
                PQfinish(pgh);
                exit(EXIT_FAILURE);
            }
        }
    } while (isPWDRequired);
    PQfinish(pgh);
    /* Get the number of available CPUs */
    numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    if (numCPUs < 1)
        numCPUs = 1;

    /*
     * Choose the number of CPUs to use in the thread pool based on load
     * average.  It only makes sense to do this if we have more than one CPU
     * to play with.
     */
    if ((numCPUs > 1)
        && (getloadavg(loadAverages, 3) == 3))
    {
        long    idleCPUs = 0;                     /* The number of idle CPUs */

        /* Show what we got */
        printf("load averages.... %3.2f %3.2f %3.2f\n",
            loadAverages[0], loadAverages[1], loadAverages[2]);

        /*
         * We're going to base the number of usable CPUs by subtracting
         * the sum of 1 (to account for OS and I/O overhead) plus the 1 minute
         * load average from the number of available CPUs.
         */
        idleCPUs = numCPUs - (1 + (int)(loadAverages[0] + 0.5));

        /* Assign # of available CPUs with some sanity checking */
        if (idleCPUs < numCPUs)
            numCPUs = idleCPUs;
        if (numCPUs < 1)
            numCPUs = 1;
    }

    /* Inform user of # of CPUs that will be used */
    printf("usable CPUs...... %d\n", numCPUs);

    /* If we have more than one CPU, multi-thread our operations */
    if (numCPUs > 1)
    {
        /* Initialize the thread pool */
        thp = tpool_init(numCPUs, 1024, true);
    }

    if (isProfiling)
        BuildCacheProfile();
    else /* isPriming */
        PrimeCache();

    /* If we have more than one CPU, multi-thread our operations */
    if (POINTER_IS_VALID(thp))
    {
        /* Destroy the thread pool */
        tpool_destroy(thp, 1); 
    }

    /* Cleanup */
    free(dbFileName);

    return EXIT_SUCCESS;

} /* main() */

/* ========================================================================= */
/* << PRIVATE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

static char *
xstrdup (const char *s)
{
    char *result;

    result = strdup(s);
    if (!result)
    {
        fprintf(stderr, "xstrdup: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return result;
} /* xstrdup() */

/* ------------------------------------------------------------------------- */

static void
usage (void)
{
    printf("primr performs Linux & Postgres cache profiling and priming.\n\n");
    printf("Usage:\n");
    printf("  primr [OPTION]...\n");
    printf("\nOptions:\n");
    printf(" [-h, --help]                       display this help and exit\n");
    printf(" [-d, --debug]                      enable runtime debugging\n");
    printf(" [-o, --postgres-only]              Postgres Buffer Cache Only\n");
    printf(" [-p, --profile]                    run in cache profiling mode\n");
    printf(" [-w, --prime]                      run in cache priming (warming) mode\n");
    printf(" [-c, --connect-string=]\"STRING\"    the Postgres connect string\n");
    printf(" [-D, --data-dir=]FILE              the Postgres data directory\n");
    printf(" [-s, --sqlite-db=]FILE             the SQLite database\n");
    printf("\nExamples:\n\n");
    printf("  # To perform cache profiling for the production database\n");
    printf("  primr -s /tmp/db10cache.sdb -c \"user=postgres dbname=production\" -D /usr/local/pgsql/data -p\n\n");
    printf("  # To perform cache priming for the production database\n");
    printf("  primr -s /tmp/db10cache.sdb -c \"user=postgres dbname=production\" -w\n\n");

} /* usage() */

/* ------------------------------------------------------------------------- */

int
busyHandler (void *pArg1, int iPriorCalls)
{
    usleep((rand() % 5000) + 5000);
    return 1;
}

/* ------------------------------------------------------------------------- */

/**
 * Retrieve the blocks of the given file currently in the Linux Page Cache.
 *
 * @param   str                 IN      The string array.
 * @param   max                 IN      The number of elements in the array.
 * @param   value               IN      The search value.
 * @returns index of the array where the value is located or -1 if not found.
 */
void
BuildCacheProfile (void)
{
    PGconn         *pgh = NULL;                /* Postgres Connection Handle */
    PGresult       *pgrsh = NULL;
    sqlite3        *dbh = NULL;                    /* SQLite Database Handle */
    sqlite3_stmt   *sth = NULL;                   /* SQLite Statement Handle */
    int             i = 0;                               /* Generic Iterator */
    int             rc = 0;                            /* SQLite Return Code */
    int             dbOid = 0;                      /* Postgres Database OID */
    int             dbTablespaceOid = 0;  /* Postgres Default Tablespace OID */
    char           *baseDir = get_current_dir_name();

    /* Open the SQLite database */
    rc = sqlite3_open(dbFileName, &dbh);

    sqlite3_busy_handler(dbh, busyHandler, NULL);

    /* Connect to Postgres */
    pgh = PQsetdbLogin(NULL, NULL, NULL, NULL,
        pgConnectString == NULL ? "postgres"
                                : pgConnectString,
        NULL, pgPassword);
    if (PQstatus(pgh) == CONNECTION_BAD)
    {
        ERROR_PRINT("%s", "Connection Test Failed\n");
        ERROR_PRINT("SQLERRMC: %s\n", PQerrorMessage(pgh));
        PQfinish(pgh);
        exit(EXIT_FAILURE);
    }

    /* Retrieve the Postgres block size */
    pgrsh = PQexec(pgh, PG_BLCKSZ_QUERY);
    if (PQresultStatus(pgrsh) == PGRES_TUPLES_OK)
    {
        db_block_size = atoi(PQgetvalue(pgrsh, 0, 0));
        PQclear(pgrsh);

        /* Inform user of block sizes */
        printf("PG BLCKSZ........ %u\n", db_block_size);
        printf("OS Page Size..... %u\n", (uint32_t)getpagesize());
    }
    else
    {
        ERROR_PRINT("%s", "Could not retrieve database settings\n");
        ERROR_PRINT("SQLERRMC: %s\n", PQerrorMessage(pgh));
        PQclear(pgrsh);
        PQfinish(pgh);
    }

    /* Retrieve the Postgres block size */
    pgrsh = PQexec(pgh, PG_DEFAULT_TABLESPACE_QUERY);
    if (PQresultStatus(pgrsh) == PGRES_TUPLES_OK)
    {
        dbOid = atoi(PQgetvalue(pgrsh, 0, 0));
        dbTablespaceOid = atoi(PQgetvalue(pgrsh, 0, 1));
        PQclear(pgrsh);
    }
    else
    {
        ERROR_PRINT("%s", "Could not retrieve database settings\n");
        ERROR_PRINT("SQLERRMC: %s\n", PQerrorMessage(pgh));
        PQclear(pgrsh);
        PQfinish(pgh);
    }

    /* Retrieve the Postgres tablespace information */
    pgrsh = PQexec(pgh, PG_OBJECT_QUERY);
    if (PQresultStatus(pgrsh) == PGRES_TUPLES_OK)
    {
        int numRows = PQntuples(pgrsh);
        char filePath[1024];

        printf("\nBeginning cache profiling for %d database objects.\n",
            numRows);

        for (i = 0; i < numRows; i++)
        {
            struct dirent  *dp = NULL;            /* Directory Entry Pointer */
            DIR            *dfd = NULL;                  /* Directory Stream */
            char pattern1[64];
            char pattern2[64];

            char *name = PQgetvalue(pgrsh, i, 0);
            char *kind = PQgetvalue(pgrsh, i, 1);
            int tableSpace = atoi(PQgetvalue(pgrsh, i, 2));
            int fileNode = atoi(PQgetvalue(pgrsh, i, 3));

            snprintf(pattern1, sizeof(pattern1), "%d", fileNode);
            snprintf(pattern2, sizeof(pattern2), "%d.*", fileNode);

            if (tableSpace == 0 || tableSpace == dbTablespaceOid)
                snprintf(filePath, sizeof(filePath),
                         "%s/pg_tblspc/%d/%d", pgDataDir,
                         dbTablespaceOid, dbOid);

            dfd = opendir(filePath);
            if (dfd != NULL)
            {
                chdir(filePath);

                while ((dp = readdir(dfd)) != NULL)
                {
                    struct stat sb;

                    /* Attempt to parse the given PG log file */
                    if (stat(dp->d_name, &sb) == -1)
                        continue;

                    /* Make sure this is a normal file */
                    if (!S_ISREG(sb.st_mode))
                        continue;

                    /* Does this file match our pattern */
                    DEBUG_PRINT("Checking pattern [%s] and [%s] for file [%s]\n",
                        pattern1, pattern2, dp->d_name);
                    if (fnmatch(pattern1, dp->d_name, 0) == 0
                        || fnmatch(pattern2, dp->d_name, 0) == 0)
                    {
                        char fullFilePath[1024];

                        snprintf(fullFilePath, sizeof(fullFilePath),
                            "%s/%s", filePath, dp->d_name);
                        if (POINTER_IS_VALID(thp))
                            tpool_add_work(thp, ProfileCacheForFile,
                                (void *)strdup(fullFilePath));
                        else
                            ProfileCacheForFile(strdup(fullFilePath));
                    }
                }

                /* We're done, close the directory stream */
                closedir(dfd);
                chdir(baseDir);
            }

        }
        PQclear(pgrsh);
    }
    else
    {
        ERROR_PRINT("%s", "Could not retrieve database settings\n");
        ERROR_PRINT("SQLERRMC: %s\n", PQerrorMessage(pgh));
        PQclear(pgrsh);
        PQfinish(pgh);
    }

    /* We're done with the connection */
    PQfinish(pgh);

} /* BuildCacheProfile() */

/* ------------------------------------------------------------------------- */

/**
 * Retrieve the blocks of the given file currently in the Linux Page Cache.
 *
 * @param   str                 IN      The string array.
 * @param   max                 IN      The number of elements in the array.
 * @param   value               IN      The search value.
 * @returns index of the array where the value is located or -1 if not found.
 */
void
PrimeCache (void)
{
    sqlite3        *dbh = NULL;                           /* Database Handle */
    sqlite3_stmt   *sth = NULL;                   /* SQLite Statement Handle */
    int             rc = 0;                            /* SQLite Return Code */

    printf("\nBeginning Cache Priming\n");
    fflush(stdout);

    /* Enable the SQLite shared cache */
//    sqlite3_enable_shared_cache(1);

    /* Open the SQLite database */
    rc = sqlite3_open(dbFileName, &dbh);

    sqlite3_busy_handler(dbh, busyHandler, NULL);

    /* Prepare the SQL */
    rc = sqlite3_prepare(dbh, SQL_CACHED_FILES_QUERY, -1, &sth, NULL);

    /*
     * Iterate over the block ranges, reading them into cache.
     */
    while ((rc = sqlite3_step(sth)) != SQLITE_ERROR)
    {
        if (rc == SQLITE_ROW)
        {
            int fileId = sqlite3_column_int(sth, 0);

            if (POINTER_IS_VALID(thp))
                tpool_add_work(thp, PrimeCacheForFile, (void *)fileId);
            else
                PrimeCacheForFile(fileId);
        }
        else if (rc == SQLITE_DONE)
        {
            break;
        }
        else if (rc == SQLITE_BUSY)
        {
            continue;
        }
    }

    sqlite3_finalize(sth);
    sqlite3_close(dbh);

} /* PrimeCache() */

/* ------------------------------------------------------------------------- */

/**
 * Retrieve the blocks of the given file currently in the Linux Page Cache.
 *
 * @param   str                 IN      The string array.
 * @param   max                 IN      The number of elements in the array.
 * @param   value               IN      The search value.
 * @returns index of the array where the value is located or -1 if not found.
 */
void
ProfileCacheForFile (char *filename)
{
    int             fd;
    struct stat     st;
    void           *pa = (char *) 0;
    char           *vec = (char *) 0;
    size_t          n = 0;
    size_t          pageSize = getpagesize();
    size_t          pageIndex;
    sqlite3        *dbh = NULL;                           /* Database Handle */
    sqlite3_stmt   *sth = NULL;                   /* SQLite Statement Handle */
    int             fileId = 0;    /* Unique ID of this file in the cache db */
    int             rc = 0;                            /* SQLite Return Code */
    char           *zSQL = NULL;                         /* SQL Query String */
    size_t          dbPageNumber = 0;

    DEBUG_PRINT("Checking cache profile for [%s]\n", filename);

    fd = open(filename, 0);
    if (0 > fd)
    {
        perror("ProfileCacheForFile open()");
        return;
    }

    if (0 != fstat(fd, &st))
    {
        perror("fstat");
        close(fd);
        return;
    }

    /* If this file is zero bytes, bail! */
    if (st.st_size == 0)
    {
        close(fd);
        return;
    }

    pa = mmap((void *) 0, st.st_size, PROT_NONE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == pa)
    {
        perror("mmap");
        close(fd);
        return;
    }

    /* vec = calloc(1, 1+st.st_size/pageSize); */
    vec = calloc(1, (st.st_size + pageSize - 1) / pageSize);
    if ((void *) 0 == vec)
    {
        perror("calloc");
        close(fd);
        return;
    }

    /* Retrieve the list of all pages in core memory (Linux Page Cache) */
    if (0 != mincore(pa, st.st_size, vec))
    {
        fprintf(stderr, "mincore(%p, %lu, %p): %s\n",
                 pa,(unsigned long) st.st_size, vec, strerror(errno));
        free(vec);
        close(fd);
        return;
    }

    /* last_insert_rowid() */
    /* handle the results */
//    for (pageIndex = 0; pageIndex <= st.st_size / pageSize; pageIndex++)
    for (pageIndex = 0, dbPageNumber = 0
         ; pageIndex <= st.st_size / pageSize
         ; pageIndex+=(db_block_size/pageSize), dbPageNumber++)
    {
        if (vec[pageIndex] & 0x1)
        {
            if (fileId == 0)
            {
                /* Open the SQLite database */
                rc = sqlite3_open(dbFileName, &dbh);

                sqlite3_busy_handler(dbh, busyHandler, NULL);

                /* Add this file to the SQLite database */
                zSQL = sqlite3_mprintf(SQL_ADD_CACHED_FILE, filename);
                sqlite3_exec(dbh, zSQL, NULL, NULL, NULL);

                /* Begin a transaction */
                sqlite3_exec(dbh, "BEGIN TRANSACTION", NULL, NULL, NULL);

                sqlite3_prepare(dbh, SQL_LAST_INSERTED_FILE_ID_QUERY, -1, &sth, NULL);
                rc = sqlite3_step(sth);
                fileId = sqlite3_column_int(sth, 0);

                sqlite3_finalize(sth);

                /* Get a new statement */
                sqlite3_prepare(dbh, SQL_FILE_PAGE_TO_CACHE, -1, &sth, NULL);
            }

            /* Bind this file's unique identifier */
            rc = sqlite3_bind_int(sth, 1, fileId);
            if (rc != SQLITE_OK)
            {
                ERROR_PRINT("%s\n", "sqlite3_bind_int/INSERT");
            }

            /* Bind the number of this page */
            rc = sqlite3_bind_int(sth, 2, dbPageNumber);
            if (rc != SQLITE_OK)
            {
                ERROR_PRINT("%s\n", "sqlite3_bind_int/INSERT (pageIndex)");
            }

            /* Insert the row */
            rc = sqlite3_step(sth);
            if (rc != SQLITE_DONE)
            {
                ERROR_PRINT("%s\n", "sqlite3_bind_int/INSERT STEP");
                ERROR_PRINT("%s\n", sqlite3_errmsg(dbh));
            }
            sqlite3_reset(sth);
        }
    }

    if (fileId != 0)
    {
        sqlite3_exec(dbh, "COMMIT TRANSACTION", NULL, NULL, NULL);
        sqlite3_finalize(sth);
        sqlite3_free(zSQL);
        sqlite3_close(dbh);
    }

    free(vec);
    vec = (char *) 0;

    munmap(pa, st.st_size);
    close(fd);

    free(filename);
    return;
}

/* ------------------------------------------------------------------------- */

/**
 * A simple binary search function.
 *
 * @param   str                 IN      The string array.
 * @param   max                 IN      The number of elements in the array.
 * @param   value               IN      The search value.
 * @returns index of the array where the value is located or -1 if not found.
 */
void
PrimeCacheForFile (void *fileNumber)
{
    int             fileId = (int *)fileNumber;
    uint32_t        pageIndex;
    char           *tempBuffer = NULL;                   /* Temporary Buffer */
    int             fd = 0;                               /* File Descriptor */
    sqlite3        *dbh = NULL;                           /* Database Handle */
    sqlite3_stmt   *sth = NULL;                   /* SQLite Statement Handle */
    int             rc = 0;                            /* SQLite Return Code */
    char           *zSQL = NULL;                         /* SQL Query String */
    char           *fileName = NULL;

    /* Open the SQLite database */
    rc = sqlite3_open(dbFileName, &dbh);

    sqlite3_busy_handler(dbh, busyHandler, NULL);

    /* Allocate a temporary buffer to read page contents into */
    tempBuffer = malloc(db_block_size * 32);

    /* Get the file name */
    rc = sqlite3_prepare(dbh, SQL_CACHED_FILE_QUERY, -1, &sth, NULL);
    if (rc != SQLITE_OK)
    {
        ERROR_PRINT("%s\n", sqlite3_errmsg(dbh));
    }
    rc = sqlite3_bind_int(sth, 1, fileId);
    if (rc != SQLITE_OK)
    {
        ERROR_PRINT("%s\n", sqlite3_errmsg(dbh));
    }
    rc = sqlite3_step(sth);
    if (rc != SQLITE_ROW)
    {
        ERROR_PRINT("%s\n", sqlite3_errmsg(dbh));
    }
    fileName = strdup(sqlite3_column_text(sth, 0));
    sqlite3_finalize(sth);

    /* Open the file-to-warm */
    fd = open(fileName, O_RDONLY);
    if (fd < 0)
    {
        perror("PrimeCacheForFile open()");
        return;
    }

    /* Get the page cache entries for this file */
    zSQL = sqlite3_mprintf(SQL_PAGE_CACHE_RANGE_QUERY,
        fileId, fileId, fileId, fileId);

    /* Prepare the SQL */
    rc = sqlite3_prepare(dbh, zSQL, -1, &sth, NULL);

    /*
     * Iterate over the block ranges, reading them into cache.
     */
    while ((rc = sqlite3_step(sth)) != SQLITE_ERROR)
    {
        if (rc == SQLITE_ROW)
        {
            int startBlock = sqlite3_column_int(sth, 0);
            int endBlock = sqlite3_column_int(sth, 1);
            int numBlocks = (endBlock - startBlock) + 1;
            size_t bytesToRead = numBlocks * db_block_size;

            DEBUG_PRINT("Thread %u is reading %d blocks (%d-%d/%u bytes)\n",
                (uint32_t)pthread_self(), numBlocks, startBlock, endBlock,
                bytesToRead);

            if (numBlocks > 1)
            {
                posix_fadvise(fd, startBlock * db_block_size, bytesToRead,
                              POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
            }
            else
            {
                posix_fadvise(fd, startBlock * db_block_size, db_block_size,
                              POSIX_FADV_RANDOM | POSIX_FADV_WILLNEED);
            }

            /*
             * If the current range consists of more than 32 blocks, we'll
             * chunk the reads into multiple, 32-block reads.
             */
            if (numBlocks > 32)
            {
                int bytesPerRead = 32 * db_block_size;
                int i;

                for (i = 0; i < numBlocks; i+=32)
                {
                    if ((numBlocks - i) < 32)
                        pread(fd, tempBuffer, (numBlocks - i) * db_block_size,
                            (startBlock + i) * db_block_size);
                    else
                        pread(fd, tempBuffer, bytesPerRead,
                            (startBlock + i) * db_block_size);
                }

            }
            else
            {
                /* Perform the actual read */
                pread(fd, tempBuffer, bytesToRead, startBlock * db_block_size);
            }
        }
        else if (rc == SQLITE_DONE)
        {
            break;
        }
        else if (rc == SQLITE_BUSY)
        {
            continue;
        }
    }

    sqlite3_finalize(sth);
    sqlite3_free(zSQL);
    sqlite3_close(dbh);

    /* Cache warmed for file */
    close(fd);

    /* Free our buffer */
    free(tempBuffer);
    free(fileName);

    return;
}

/* vi: set et sw=4 ts=4: */

