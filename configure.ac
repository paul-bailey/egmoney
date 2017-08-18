AC_PREREQ([2.69])
AC_INIT([egmoney],[0.1],[pbailey@highlandtechnology.com])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/m4])
AC_CONFIG_SRCDIR([etc/daemon_thoughts.c])
AC_CONFIG_HEADERS([config.h])

LT_PREREQ([2.2])
LT_INIT([dlopen])

AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_CC_C_O

fail_headers='eg-devel'
for i in $fail_headers; do
  AC_CHECK_HEADER([${i}.h], ,[AC_MSG_ERROR([${i}.h missing])])
done

fail_funcs=''
for i in $fail_funcs; do
  AC_CHECK_FUNC([${i}], ,[AC_MSG_ERROR([Symbol ${i} missing])])
done

fail_libs='eg-devel m'
for i in $fail_libs; do
  AC_CHECK_LIB([${i}], eg_parse_date, ,[AC_MSG_ERROR([lib${i} missing])])
done

AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_CONFIG_FILES([Makefile
                 lib/Makefile
                 scripts/Makefile
                 include/Makefile
                 egm-cli/Makefile])

AC_OUTPUT


echo \
"-------------------------------------------------

${PACKAGE_NAME} Version ${PACKAGE_VERSION}

Prefix: '${prefix}'.
Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'

Now type 'make @<:@<target>@:>@'
    where the optional <target> is:
        all         - build all binaries
        install     - install everything

--------------------------------------------------"
