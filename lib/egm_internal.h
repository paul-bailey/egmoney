#ifndef EGM_INTERNAL_H
#define EGM_INTERNAL_H

#include <egmoney.h>
#include <eglist.h>
#include <egxml.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef arraylen
# define arraylen(a_) (sizeof(a_) / sizeof((a_)[0]))
#endif

#ifdef __GNUC__
# define hidden__ __attribute__((visibility("hidden")))
# define EXPORT   __attribute__((visibility("default")))
#else
# define hidden__
# define EXPORT
#endif

#define JOURNAL_DB              "journal.xml"
#define ACCT_DB                 "accounts.xml"
#define JOURNAL_DB_BACKUP       "journal.backup.xml"
#define ACCT_DB_BACKUP          "accounts.backup.xml"

/**
 * struct journal_trans_t - A single-entry transaction
 * @amt: Amount. If positive, it's a debit; if negative, it's a credit
 * @acct: Account number
 *
 * A journal entry has some number of these, whose values add up to zero.
 */
struct journal_trans_t {
        struct list_head list;
        float amt;
        /* TODO: change from 'int32_t acct' (ID) to 'const struct acct_t *acct' */
        int32_t acct;
};

#define J_TRANS_SIZE \
        (EGM_TRANSPERENTRY * sizeof(struct journal_trans_t))

#define J_RESVD_SIZE \
        (512 - (4 * 8 + J_TRANS_SIZE))

/**
 * struct egentry_t - book entry.
 * @j_notes: String describing the transaction
 * @j_trans: Affected accounts and amount of transactions to each
 * @j_laststamp: Previous entry's @j_stamp field
 * @j_date: Date of transaction
 * @j_stamp: Unix date stamp of this entry being created
 *
 * The @j_hash and @j_laststamp fields are not used by egm-cli.  They are
 * for porting to egb-cli.
 *
 * WARNING: This, along with &struct journal_trans_t, are part of the
 * ABI so don't edit it unless you know what you're doing.
 */
struct egentry_t {
        char *j_notes;
        struct list_head j_list;
        struct list_head j_trans;
        uint64_t j_date;
        uint64_t j_stamp;
};

#define trans_foreach(iter_, entry_) \
        list_for_each_entry(iter_, &(entry_)->j_trans, list)
#define trans_foreach_safe(iter_, tmp_, entry_) \
        list_for_each_entry_safe(iter_, tmp_, &(entry_)->j_trans, list)

#define ACCT_MODIFIED (0x0001)
struct acct_t {
        char *name;
        int id;
        struct list_head *parent;
        struct list_head siblings;
        struct list_head children;
        struct list_head unordered;
        char *notes;
        float sum;
        unsigned int flags;
};

struct egmoney_t {
        struct list_head entry_list;
        struct list_head acct_list; /* fast-access account list */
        struct tm start;
        struct tm end;
        struct acct_t *master_acct; /* hierarchical account tree */
        char path[2048];
        int range;
};

/*
 * Name conventions used in this library's code:
 * "journal" refers to the entire ledger
 * "entry" refers to a single entry in the journal
 * "trans" refers to a single account's line in an entry
 */

/* Keep oldest error, unless you're clearing it. */
static inline void
egm_set_errno(int err)
{
        if (egm_errno == 0 || err == 0)
                egm_errno = err;
}

/* common to xxx_writedb.c */
static inline void
add_attr(XmlTag *tag, const char *name, const char *val)
{
        if (xml_add_attribute(tag, name, val) != 0)
                egm_set_errno(EGM_ERRNO);
}

/* err.c */
extern void egm_syntax__(FILE *fp);

/* egmio.c */
extern int egm_acct_sync__(Egmoney *egm);

/* date_range.c TODO: rename like "helpers.c" */
extern int egm_in_range__(Egmoney *egm, Egentry *entry);
extern int egm_verify_xml__(FILE *fp);

/* journal.c */
extern void egm_journal_cleandb__(Egmoney *egm);
extern void egm_trans_destroy__(struct journal_trans_t *trans);
extern struct journal_trans_t *egm_trans_create__(void);
extern Egentry *egm_entry_create__(void);
extern void egm_entry_destroy__(Egentry *entry);
extern void egm_journal_cleandb__(Egmoney *egm);

/* journal_readdb.c */
extern int egm_journal_readdb__(FILE *fp, void *priv, XmlTag *tag);

/* journal_writedb.c */
extern int egm_journal_writedb__(FILE *fp, int state, void *priv);

/* acct.c */
extern int egm_acct_editable__(Egmoney *egm, int acct_no);
extern void egm_acct_destroy__(struct acct_t *a);
static inline struct acct_t *acct_parent(struct acct_t *child)
{
        if (!child->parent)
                return NULL;
        return list_entry(child->parent, struct acct_t, children);
}
static inline void acct_add_parent(struct acct_t *child, struct acct_t *parent)
{
        child->parent = &parent->children;
        list_add(&child->siblings, &parent->children);
}

/* acct_readdb.c */
extern int egm_account_readdb__(FILE *fp, void *priv, XmlTag *tag);

/* acct_writedb.c */
extern int egm_account_writedb__(FILE *fp, int state, void *priv);

#endif /* EGM_INTERNAL_H */
