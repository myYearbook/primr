
AM_CFLAGS = @THREADSAFE_FLAGS@ @DYNAMIC_EXTENSION_FLAGS@

lib_LTLIBRARIES = libsqlite3.la
libsqlite3_la_SOURCES = sqlite3.c
libsqlite3_la_LDFLAGS = -no-undefined -version-info 8:6:8

bin_PROGRAMS = primr
primr_SOURCES = primr.c tpool.c  -I/home/jharris/global/pg/postgresql-8.3.4-x86_64/install/include
primr_LDADD = libsqlite3.la -L/home/jharris/global/pg/postgresql-8.3.4-x86_64/install/lib -lpq -lpgport
primr_DEPENDENCIES = libsqlite3.la

include_HEADERS = sqlite3.h sqlite3ext.h

