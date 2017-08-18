#include "egm_internal.h"
#include <eg-devel.h>
#include <egxml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct e2xml_t {
        Egentry *entry;
        Egmoney *egm;
        struct journal_trans_t *trans;
};

static void
stamp2str_(time_t t, char *buf, size_t size)
{
        char *ret;
        snprintf(buf, size, "%lu", (unsigned long)t);
        buf[size - 1] = '\0';
}

#define stamp2str(t_,buf_) stamp2str_((t_),(buf_),sizeof(buf_))

static int
print_notes_cb(FILE *fp, int state, void *priv)
{
        const char *notes = priv;

        xml_fprints(fp, notes);
        putc('\n', fp);
        return 0;
}

/* helper to print_entry_cb */
static int
print_notes(FILE *fp, int state, const char *notes)
{
        int res;
        struct xml_runner_t runner;

        runner.flags = XML_NL,
        runner.priv  = (char *)notes,
        runner.cb    = print_notes_cb,
        runner.tag   = xml_tag_create("notes");
        if (!runner.tag)
                return -1;
        res = xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
        return res;
}

/* helper to print_entry_cb */
static int
print_trans(FILE *fp, int state, struct journal_trans_t *tr)
{
        struct xml_runner_t runner;
        char id[40];
        char val[40];
        int res = -1;

        runner.flags = XML_NL,
        runner.priv  = tr,
        runner.cb    = NULL,
        runner.tag   = xml_tag_create("account");
        if (!runner.tag)
                goto etag;

        snprintf(id, sizeof(id), "%d", tr->acct);
        snprintf(val, sizeof(val), "%.3f", tr->amt);
        id[sizeof(id)-1] = '\0';
        val[sizeof(val)-1] = '\0';

        add_attr(runner.tag, "id", id);
        add_attr(runner.tag, "value", val);
        if (egm_errno)
                goto eattr;

        res = xml_tag_recursive(fp, state, &runner);
eattr:
        xml_tag_free(runner.tag);
etag:
        return res;
}

static int
print_entry_cb(FILE *fp, int state, void *priv)
{
        Egentry *entry = priv;
        struct journal_trans_t *tr;
        const char *notes;
        trans_foreach(tr, entry) {
                if (print_trans(fp, state, tr) < 0)
                        return -1;
        }
        notes = egm_entry_get_notes(entry);
        if (notes != NULL)
                return print_notes(fp, state, notes);
        return 0;
}

static int
print_entry(FILE *fp, int state, Egentry *entry)
{
        struct xml_runner_t runner;
        char date[40], stamp[40];
        int res = -1;

        runner.flags = XML_NL;
        runner.priv  = entry;
        runner.cb    = print_entry_cb;
        runner.tag   = xml_tag_create("entry");
        if (!runner.tag)
                return -1;

        stamp2str(entry->j_date, date);
        stamp2str(entry->j_stamp, stamp);
        add_attr(runner.tag, "date", date);
        add_attr(runner.tag, "stamp", stamp);
        if (!egm_errno)
                res = xml_tag_recursive(fp, state, &runner);

        xml_tag_free(runner.tag);
        return res;
}

static int
print_ledger(FILE *fp, int state, void *priv)
{
        Egmoney *egm = priv;
        Egentry *entry;

        egm_foreach_entry(egm, entry) {
                if (print_entry(fp, state, entry) < 0)
                        return -1;
        }
        return 0;
}

int hidden__
egm_journal_writedb__(FILE *fp, int state, void *priv)
{
        struct xml_runner_t runner;
        char stamp[40];
        time_t t = time(NULL);
        int res = -1;
        Egmoney *egm = priv;

        runner.flags = XML_NL,
        runner.priv  = egm,
        runner.cb    = print_ledger,
        runner.tag   = xml_tag_create("ledger");
        if (!runner.tag)
                return -1;

        stamp2str(t, stamp);
        add_attr(runner.tag, "stamp", stamp);
        add_attr(runner.tag, "dbpath", egm_path(egm));
        if (!egm_errno)
                res = xml_tag_recursive(fp, 1, &runner);
        return res;
}
