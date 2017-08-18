#include "egm_internal.h"
#include <egmoney.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

int EXPORT egm_errno = 0;

/**
 * egm_strerror - Get a string describing the error.
 * @err: Usually just egm_errno
 */
const char EXPORT *
egm_strerror(int err)
{
        switch (err) {
        case EGM_DBORIG:
                return "Database does not match hard-coded original";
        case EGM_DBFAIL:
                return "Database read error";
        case EGM_ERRNO:
                return errno ? strerror(errno) : "System error";
        case EGM_INVAL:
                return "Invalid argument";
        case EGM_EACCT:
                return "Invalid account number";
        case EGM_EFULL:
                return "Too many transaction for entry";
        case EGM_ESYNC:
                return "Database write error";
        case EGM_EBACK:
                return "Failed to back up database; refusing to overwrite old";
        case EGM_ESYNTAX:
                return "Database syntax error";
        default:
                return "Undefined error";
        }
}

static char *sred = "";
static char *syel = "";
static char *soff = "";
static FILE *logfp = NULL;

/**
 * egm_perror - Like perror, but better and egm-specific.
 * @fmt: The formatted message to print to standard error.
 */
void EXPORT
egm_perror(const char *fmt, ...)
{
        va_list ap;
        int err = errno;

        if (logfp == NULL)
                egm_setlog(stderr);

        fprintf(logfp, "%s", sred);
        va_start(ap, fmt);
        vfprintf(logfp, fmt, ap);
        va_end(ap);
        fprintf(logfp, "%s", syel);

        if (egm_errno != 0)
                fprintf(logfp, " (%s)", egm_strerror(egm_errno));
        if (egm_errno != EGM_ERRNO && err != 0)
                fprintf(logfp, " (%s)", strerror(err));
        fprintf(logfp, "%s", soff);
        fprintf(logfp, "\n");
}

void EXPORT
egm_setlog(FILE *fp)
{
        if (isatty(fileno(fp))) {
                sred = "\e[31;1m";
                syel = "\e[33;1m";
                soff = "\e[0m";
        } else {
                sred = "";
                syel = "";
                soff = "";
        }
        logfp = fp;
}

void hidden__
egm_syntax__(FILE *fp)
{
        int err = errno;
        long pos, errpos;
        long count;
        int lineno;
        char *res;
        char line[1024];

        err = errno;
        if (err != 0) {
                /* An egxml.h function failed due to a system failure */
                egm_set_errno(EGM_DBFAIL);
                return;
        }

        /* else, there was a syntax error in the database file */
        egm_set_errno(EGM_ESYNTAX);
        errpos = ftell(fp);
        if (errpos < 0)
                goto nosyntax;

        rewind(fp);
        /* XXX: tedious */
        for (lineno = 0, pos = 0; pos < errpos; lineno++) {
                res = fgets(line, sizeof(line), fp);
                if (res == NULL)
                        goto nosyntax;
                pos = ftell(fp);
                if (pos < 0)
                        goto nosyntax;
        }

        if (logfp == NULL)
                egm_setlog(stderr);

        line[sizeof(line)-1] = '\0';
        fprintf(logfp, "libegmoney: %sERROR:%s Database syntax error near line %d:\n",
                sred, soff, lineno);
        fprintf(logfp, "\t'%s'\n", line);
        fseek(fp, errpos, SEEK_SET);

        errno = err;
        return;

nosyntax:
        fprintf(logfp, "%sDatabase syntax error%s\n", sred, soff);
        errno = err;
}
