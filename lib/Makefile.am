lib_LTLIBRARIES    	= libegmoney.la
libegmoney_la_SOURCES 	= \
 acct.c \
 acct_readdb.c \
 acct_writedb.c \
 journal_readdb.c \
 journal_writedb.c \
 date_range.c \
 egm_internal.h \
 egmio.c \
 egm_struct.c \
 err.c \
 journal.c
libegmoney_la_LDFLAGS = -version-info 0:0:0
libegmoney_la_CPPFLAGS = \
 -I$(top_srcdir)/include \
 -DDATADIR=\"$(datadir)/egm\" \
 -fvisibility=hidden
