#include "egm_internal.h"
#include <eg-devel.h>
#include <egxml.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int
print_notes_cb(FILE *fp, int state, void *priv)
{
        int c;
        struct acct_t *a = priv;
        char *src = a->notes;
        xml_fprints(fp, a->notes);
        return 0;
}

static int
print_notes(FILE *fp, int state, struct acct_t *a)
{
        struct xml_runner_t runner;
        int res;

        /* bail early if we got nothing to print */
        if (!a->notes)
                return 0;

        runner.flags = XML_NL;
        runner.priv = a;
        runner.cb = print_notes_cb;
        runner.tag = xml_tag_create("notes");
        if (runner.tag == NULL)
                return -1;

        res = xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
        return res;
}

static int account2xml_r(FILE *fp, int state, struct acct_t *a);

static int
print_account(FILE *fp, int state, void *priv)
{
        struct acct_t *child, *parent;
        parent = priv;
        print_notes(fp, state, parent);
        list_for_each_entry_reverse(child, &parent->children, siblings) {
                if (account2xml_r(fp, state, child) != 0)
                        return -1;
        }
        return 0;
}

static int
account2xml_r(FILE *fp, int state, struct acct_t *a)
{
        struct xml_runner_t runner;
        char id[12];
        int res = -1;

        snprintf(id, sizeof(id), "%d", a->id);
        id[sizeof(id)-1] = '\0';

        runner.flags = XML_NL;
        runner.priv = a;
        runner.cb = list_is_empty(&a->children) ? NULL : print_account;
        runner.tag = xml_tag_create("account");
        if (runner.tag == NULL)
                return -1;
        add_attr(runner.tag, "id", id);
        add_attr(runner.tag, "name", a->name);
        if (!egm_errno)
                res = xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
        return res;
}

int hidden__
egm_account_writedb__(FILE *fp, int state, void *priv)
{
        return account2xml_r(fp, 1, ((Egmoney *)priv)->master_acct);
}
