#include "egm_internal.h"
#include <eg-devel.h>
#include <egxml.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

struct acct_parse_t {
        struct acct_t *a;
        Egmoney *egm;
};

static struct acct_t *
account_create(struct acct_t *parent)
{
        struct acct_t *a = malloc(sizeof(*a));
        if (!a)
                return NULL;

        init_list_head(&a->siblings);
        init_list_head(&a->unordered);
        init_list_head(&a->children);
        if (parent != NULL)
                acct_add_parent(a, parent);
        else
                a->parent = NULL;
        a->name = NULL;
        a->id = -1;
        a->notes = NULL;
        a->sum = 0.0;
        a->flags = 0;
        return a;
}

static int
parse_name_attr(void *priv, const char *name)
{
        struct acct_t *a = priv;
        if (a->name != NULL)
                return -1;
        a->name = strdup(name);
        if (!a->name)
                return -1;
        return 0;
}

static int
parse_id_attr(void *priv, const char *idstr)
{
        struct acct_t *a = priv;
        char *endptr;
        if (a->id >= 0)
                return -1;
        a->id = strtoul(idstr, &endptr, 10);
        if (endptr == idstr || errno) {
                errno = 0;
                return -1;
        }
        return 0;
}

static int
fill_notes(FILE *fp, struct acct_t *a, XmlTag *tagbuf)
{
        int c;
        String *str = string_create(NULL);
        int res = -1;
        if (!str)
                return -1;
        while ((c = getc(fp)) != '<') {
                if (c == EOF)
                        goto err;
                c = string_putc(str, c);
                if (c == EOF)
                        goto err;
        }
        ungetc('<', fp);
        a->notes = strdup(string_cstring(str));
        if (a->notes == NULL)
                goto err;
        res = 0;
err:
        string_destroy(str);
        return res;
}

static int
parse_notes_elem(FILE *fp, void *priv, XmlTag *tag)
{
        struct acct_parse_t *ap = priv;
        if (ap->a->notes != NULL)
                return -1;
        else if (fill_notes(fp, ap->a, tag))
                return -1;
        return 0;
}

static int
parse_acct_elem(FILE *fp, void *priv, XmlTag *tag)
{
        static const struct xml_elem_parser account_elements[] = {
                { "account", parse_acct_elem },
                { "notes", parse_notes_elem },
                { NULL, NULL },
        };
        static const struct xml_attr_parser account_attributes[] = {
                { "name", parse_name_attr },
                { "id",   parse_id_attr },
                { NULL, NULL },
        };
        struct acct_parse_t ap;
        struct acct_parse_t *ap_old = priv;
        struct acct_t *parent = ap_old->a;
        struct acct_t *a = account_create(parent);
        if (!a)
                return -1;

        list_add(&a->unordered, &ap_old->egm->acct_list);
        if (xml_parse_attributes(a, account_attributes, tag) < 0)
                goto err;

        /* siblings may not have the same name or id */
        /* TODO: ID should be a global check */
        if (parent != NULL) {
                struct acct_t *b;
                list_for_each_entry(b, &parent->children, siblings) {
                        if (b == a)
                                continue;
                        if (b->id == a->id)
                                goto err;
                        if (!strcmp(b->name, a->name))
                                goto err;
                }
        }

        if (!(xml_tag_flags(tag) & XML_END)) {
                /* "<account ... >", not "<account ... />" */
                ap.a = a;
                ap.egm = ap_old->egm;
                if (xml_parse_elements(fp, &ap, account_elements, "account") < 0)
                        goto err;
        }

        /*
         * If we're the root account we need to declare ourselves.  Note
         * that due to recursion, master_acct can be NULL if the parent
         * account is not NULL, so we only check the parent account.
         */
        if (parent == NULL)
                ap_old->egm->master_acct = a;

        return 0;

err:
        egm_acct_destroy__(a);
        return -1;
}

#define unordered_end(egm_, acct_) \
        (&(acct_)->unordered == &(egm_)->acct_list)
#define unordered_next(acct_) \
        list_next_entry(acct_, unordered)
#define unordered_first(egm_) \
        list_first_entry(&(egm_)->acct_list, struct acct_t, unordered)

/*
 * Helper to check_account_ids()
 * Recursively check all accounts, with a shrinking list forward.
 * This assumes there aren't a million accounts to overrun the stack.
 */
static int
check_account_ids_(Egmoney *egm, struct acct_t *acct)
{
        struct acct_t *acct_save;
        int idsave;

        if (unordered_end(egm, acct))
                return 0;

        /* Don't dereference acct until we know we are not at end */
        idsave = acct->id;
        acct = acct_save = unordered_next(acct);
        while (!unordered_end(egm, acct)) {
                if (acct->id == idsave) {
                        egm_perror("ERROR: Account id '%d' is not globally unique");
                        return -1;
                }
                acct = unordered_next(acct);
        }
        return check_account_ids_(egm, acct_save);
}

/* return 0 if all account ID's are globally unique */
static int
check_account_ids(Egmoney *egm)
{
        return check_account_ids_(egm, unordered_first(egm));
}

int hidden__
egm_account_readdb__(FILE *fp, void *priv, XmlTag *tag)
{
        Egmoney *egm = priv;
        struct acct_parse_t ap;
        int ret;

        /* More than one account root in file? */
        if (egm->master_acct != NULL)
                return -1;

        ap.a = NULL;
        ap.egm = egm;
        ret = parse_acct_elem(fp, (void *)&ap, tag);
        if (ret < 0)
                return -1;

        return check_account_ids(egm);
}
