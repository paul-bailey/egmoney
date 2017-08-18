bin_PROGRAMS = egm-cli
egm_cli_CPPFLAGS = -Wall -I$(top_srcdir)/include -DDATADIR=\"$(datadir)/egm\"
egm_cli_LDADD = $(top_srcdir)/lib/libegmoney.la
egm_cli_SOURCES = \
  append.c \
  html.c \
  print.c \
  egm_cli.h \
  main.c
