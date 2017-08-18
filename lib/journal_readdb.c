#include "egm_internal.h"
#include <eg-devel.h>
#include <egxml.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static int
new_trans_valid(const struct journal_trans_t *trans)
{
        /*
         * Validation of account number will occur in
         * new_entry_valid()
         */
        return isfinite(trans->amt) && trans->acct >= 0;
}

static int
new_entry_valid(Egmoney *egm, Egentry *entry)
{
        struct journal_trans_t *tr;
        if (entry->j_stamp == 0 || entry->j_date == 0
            || list_is_empty(&entry->j_trans)) {
                return false;
        }

        trans_foreach(tr, entry) {
                if (egm_acctstr(egm, tr->acct) == NULL)
                        return false;
        }
        return egm_entry_balanced(entry);
}

static long
attr2ulong(const char *attr_value)
{
        int ret;
        char *endptr;
        while (isspace((int)(*attr_value)))
                ++attr_value;
        ret = strtoul(attr_value, &endptr, 0);
        if (errno || endptr == attr_value) {
                errno = 0;
                return -1L;
        }
        return ret;
}

static int
parse_stamp_attr(void *priv, const char *value)
{
        Egentry *entry = priv;
        long stamp;
        if (entry->j_stamp != 0)
                return -1;
        stamp = attr2ulong(value);
        if (stamp < 0)
                return -1;
        entry->j_stamp = stamp;
        return 0;
}

static int
parse_date_attr(void *priv, const char *value)
{
        Egentry *entry = priv;
        long date;
        if (entry->j_date != 0)
                return -1;
        date = attr2ulong(value);
        if (date < 0)
                return -1;
        entry->j_date = date;
        return 0;
}

static int
parse_notes_attr(void *priv, const char *value)
{
        Egentry *entry = priv;
        char *notes;
        if (entry->j_notes != NULL)
                return -1;
        notes = strdup(value);
        if (!notes)
                return -1;
        entry->j_notes = notes;
        return 0;
}

static int
parse_notes_elem(FILE *fp, void *priv, XmlTag *tag)
{
        Egentry *entry = priv;
        int ret = -1;
        XmlTag *end = NULL;
        char *notes;

        if (entry->j_notes != NULL
            || (xml_tag_flags(tag) & XML_END) != 0) {
                return -1;
        }
        notes = xml_elem_get_text(fp, XML_STRIP);
        if (!notes)
                return -1;
        entry->j_notes = notes;
        end = xml_tag_parse(fp);
        if (!end)
                goto enotag;
        if (strcmp(xml_tag_name(end), "notes"))
                goto etagname;
        if (!(xml_tag_flags(end) & XML_END))
                goto etagname;
        ret = 0;
etagname:
        xml_tag_free(end);
        if (ret < 0) {
                free(notes);
                entry->j_notes = NULL;
        }
enotag:
        return ret;
}

static int
parse_id_attr(void *priv, const char *s)
{
        struct journal_trans_t *tr = priv;
        long int id;
        if (tr->acct >= 0)
                return -1;
        id = attr2ulong(s);
        if (id < 0)
                return -1;
        tr->acct = id;
        return 0;
}

static int
parse_amt_attr(void *priv, const char *s)
{
        struct journal_trans_t *tr = priv;
        char *endptr;
        float f;
        if (isfinite(tr->amt))
                return -1;
        f = strtof(s, &endptr);
        if (endptr == s || errno || !isfinite(f)) {
                errno = 0;
                return -1;
        }
        tr->amt = f;
        return 0;
}

#define ACCT_ELEM 0
static int
parse_jacct_elem(FILE *fp, void *priv, XmlTag *tag)
{
        static const struct xml_attr_parser account_attributes[] = {
                { "id", parse_id_attr },
                { "value", parse_amt_attr },
                { NULL, NULL },
        };
        const struct xml_elem_parser account_elements[] = {
#if ACCT_ELEM
                { "id", parse_id_elem },
                { "value", parse_amt_elem },
#endif /* ACCT_ELEM */
                { NULL, NULL },
        };
        Egentry *entry = priv;
        struct tagbuf_attr_t *attr;
        struct journal_trans_t *tr = egm_trans_create__();
        if (!tr)
                return -1;
        if (xml_parse_attributes(tr, account_attributes, tag) < 0)
                goto err;

        if (!(xml_tag_flags(tag) & XML_END)) {
                if (xml_parse_elements(fp, tr, account_elements, "account") < 0)
                        goto err;
        }

        if (!new_trans_valid(tr))
                goto err;

        list_add(&tr->list, &entry->j_trans);
        return 0;
err:
        egm_trans_destroy__(tr);
        return -1;
}

static int
parse_entry_elem(FILE *fp, void *priv, XmlTag *tag)
{
        static const struct xml_attr_parser entry_attributes[] = {
                { "date", parse_date_attr },
                { "stamp", parse_stamp_attr },
                { "notes", parse_notes_attr },
                { NULL, NULL },
        };
        static const struct xml_elem_parser entry_elements[] = {
                { "notes", parse_notes_elem },
                { "account", parse_jacct_elem },
                { NULL, NULL },
        };
        Egmoney *egm = priv;
        Egentry *entry = egm_entry_create__();
        if (!entry)
                return -1;
        /* Must contain at least two "account" elements */
        if (!!(xml_tag_flags(tag) & XML_END))
                goto err;

        if (xml_parse_attributes(entry, entry_attributes, tag) < 0)
                goto err;

        if (xml_parse_elements(fp, entry, entry_elements, "entry") < 0)
                goto err;

        if (!new_entry_valid(egm, entry))
                goto err;

        list_add_tail(&entry->j_list, &egm->entry_list);
        return 0;
err:
        egm_entry_destroy__(entry);
        return -1;
}

int hidden__
egm_journal_readdb__(FILE *fp, void *priv, XmlTag *tag)
{
        const struct xml_elem_parser ledger_elements[] = {
                { "entry", parse_entry_elem },
                { NULL, NULL },
        };
        Egmoney *egm = priv;

        /* Cannot parse journal before parsing accounts */
        if (egm->master_acct == NULL)
                return -1;
        /* Only one root ledger permitted */
        if (!list_is_empty(&egm->entry_list))
                return -1;

        return xml_parse_elements(fp, egm, ledger_elements, "ledger");
}
