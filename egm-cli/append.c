#include "egm_cli.h"
#include <eg-devel.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

static char *
input(const char *msg)
{
        char *line = NULL;
        size_t len = 0;
        ssize_t count;
        printf("%s: ", msg);
        count = getline(&line, &len, stdin);
        if (count < 0) {
                fprintf(stderr, "aborted\n");
                return NULL;
        }
        return line;
}

static const char *
getnotes(void)
{
        char *line = NULL;
        size_t len = 0;
        ssize_t count;
        const char *s;

        printf("Anything to say?: ");
        count = getline(&line, &len, stdin);
        if (count < 0)
                return NULL;

        s = eg_slide(line);
        if (strlen(s) == 0) {
                free(line);
                return NULL;
        }
        return line;
}

static float
getvalf(void)
{
        char *line, *endptr;
        float v;

        if ((line = input("Amount")) == NULL)
                goto inval;
        v = strtof(line, &endptr);
        if (errno || endptr == line) {
                goto inval;
        }
        free(line);
        return v;

inval:
        egm_errno = EGM_INVAL;
        fail();
        return 0.0;
}

static uint64_t
get_date(void)
{
        time_t t;
        char *line;
        char *sstart;

        if ((line = input("Date (blank=today)")) == NULL) {
                egm_errno = EGM_ERRNO;
                fail();
        }

        for (sstart = line; isspace((int)*sstart); ++sstart)
                ;
        if (sstart[0] != '\0') {
                struct tm tm;
                if (eg_parse_date(line, &tm) != 0) {
                        egm_errno = EGM_INVAL;
                        fail();
                }
                t = mktime(&tm);
        } else {
                t = time(NULL);
        }
        free(line);
        return (uint64_t)t;

}

static int
get_account(Egmoney *egm)
{
        char *line, *endptr;
        int v, ret = -1;
        while (true) {
                line = input("'?' = show accounts, -1 = no more accounts\nAccount number");
                if (!line)
                        return -1;
                if (line[0] != '?')
                        break;
                egm_print_accounts(egm, EGM_TERM, 0);
                free(line);
        }

        v = strtol(line, &endptr, 10);
        if (errno || endptr == line) {
                egm_errno = EGM_INVAL;
                fail();
        } else if (v < 0) {
                /* No more entries */
                ret = -1;
        } else {
                ret = v;
        }

        free(line);
        return ret;
}

static Egentry *
get_journal_interactive(Egmoney *egm)
{
        int count;
        Egentry *entry;
        const char *notes;

        if ((entry = egm_new_entry(get_date())) == NULL)
                fail();

        count = 0;
        for (;;) {
                struct egm_trans_t data;
                data.acct_no = get_account(egm);
                if (data.acct_no < 0)
                        break;
                data.amt = getvalf();
                if (egm_entry_add_trans(egm, entry, &data) < 0)
                        fail();
                ++count;
        }
        if (count <= 1) {
                fprintf(stderr, "Need at least one account\n");
                fail();
        }

        if ((notes = getnotes()) != NULL)
                egm_entry_add_notes(entry, notes);

        if (!egm_entry_balanced(entry)) {
                fprintf(stderr, "Unbalanced entry\n");
                fail();
        }
        return entry;
}

int
do_append(int argc, char **argv, struct egm_args_t *args)
{
        Egentry *entry;

        entry = get_journal_interactive(args->egm);
        if (egm_entry_append(args->egm, entry) < 0)
                fail();
        return EXIT_SUCCESS;
}
