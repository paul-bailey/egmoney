#include "egm_internal.h"
#include <eg-devel.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>


/* **********************************************************************
 *              Library internal functions
 ***********************************************************************/

struct journal_trans_t hidden__ *
egm_trans_create__(void)
{
        struct journal_trans_t *tr;
        tr = malloc(sizeof(*tr));
        if (!tr)
                return NULL;
        tr->acct = -1;
        tr->amt = INFINITY;
        init_list_head(&tr->list);
        return tr;
}

void hidden__
egm_trans_destroy__(struct journal_trans_t *trans)
{
        list_del(&trans->list);
        free(trans);
}

Egentry hidden__ *
egm_entry_create__(void)
{
        Egentry *entry = malloc(sizeof(*entry));
        if (!entry)
                return NULL;
        entry->j_notes = NULL;
        entry->j_stamp = 0;
        entry->j_date = 0;
        init_list_head(&entry->j_list);
        init_list_head(&entry->j_trans);

        return entry;
}

void hidden__
egm_entry_destroy__(Egentry *entry)
{
        struct journal_trans_t *tr, *tr2;
        if (entry->j_notes != NULL)
                free(entry->j_notes);
        list_del(&entry->j_list);

        trans_foreach_safe(tr, tr2, entry) {
                egm_trans_destroy__(tr);
        }
        free(entry);
}

void hidden__
egm_journal_cleandb__(Egmoney *egm)
{
        Egentry *entry, *tmp;
        list_for_each_entry_safe(entry, tmp, &egm->entry_list, j_list) {
                egm_entry_destroy__(entry);
        }
}


/* **********************************************************************
 *              Journal public functions
 ***********************************************************************/

/**
 * egm_entry_balanced - Determine if a journal entry is balanced
 * @entry: Entry to check
 *
 * Return: %true if the journal entry's DEBIT and CREDIT sum up
 * properly to zero (or less than one half of one cent).
 */
int EXPORT
egm_entry_balanced(Egentry *entry)
{
        struct journal_trans_t *tr;
        float sum = 0.000;
        int ntrans = 0;

        trans_foreach(tr, entry) {
                sum += tr->amt;
                ++ntrans;
        }

        return ntrans > 1 && sum < 0.001 && sum > -0.001;
}

int EXPORT
egm_entry_valid(Egentry *entry)
{
        /* deprecated -- no longer have checksums */
        return egm_entry_balanced(entry);
}

unsigned long EXPORT
egm_entry_stamp(Egentry *entry)
{
        return entry->j_stamp;
}

unsigned long EXPORT
egm_entry_date(Egentry *entry)
{
        return entry->j_date;
}


/**
 * egm_entry_add_trans - Add a transaction to a journal entry
 * @egm: Egmoney handle that @entry will belong to when appended, and which
 *       stores @acct_no's metadata
 *
 * Return: zero if added, -1 if not, in which egm_errno will be set.
 */
int EXPORT
egm_entry_add_trans(Egmoney *egm, Egentry *entry, const struct egm_trans_t *trans)
{
        struct journal_trans_t *tr;

        if (!egm_acct_editable__(egm, trans->acct_no))
                return -1;

        tr = egm_trans_create__();
        if (!tr)
                return -1;

        tr->acct = trans->acct_no;
        tr->amt = trans->amt;
        list_add(&tr->list, &entry->j_trans);
        return 0;
}

void EXPORT *
egm_entry_next_trans(Egentry *entry, void *last, struct egm_trans_t *trans)
{
        struct journal_trans_t *tr, *trlast;
        if (!last) {
                tr = list_first_entry_or_null(&entry->j_trans,
                                              struct journal_trans_t,
                                              list);
                if (!tr)
                        return NULL;
        } else {
                trlast = (struct journal_trans_t *)last;
                tr = list_next_entry(trlast, list);
                if (&tr->list == &entry->j_trans)
                        return NULL;
        }

        trans->amt = tr->amt;
        trans->acct_no = tr->acct;
        return (void *)tr;
}

/**
 * egm_entry_add_notes - Append a message describing the entry's transactions
 * @entry: Entry to add notes to
 * @notes: Message to write.
 */
void EXPORT
egm_entry_add_notes(Egentry *entry, const char *notes)
{
        const char *src;
        char *dst;
        int c;
        char *newnotes;
        const char *head;
        char *tail;

        /* skip leading whitespace */
        head = notes;
        while (isspace(c = *head) && c != '\0')
                ++head;

        /* empty line? */
        if (c == '\0')
                return;

        /* TODO: no return value! */
        newnotes = strdup(head);
        if (!newnotes) {
                egm_errno = EGM_ERRNO;
                return;
        }

        /* Trim trailing whitespace */
        tail = newnotes + strlen(newnotes) - 1;
        while (isspace(c = *tail) && tail > newnotes) {
                *tail = '\0';
                --tail;
        }
        entry->j_notes = newnotes;
}

/**
 * egm_enry_get_notes - Get notes from an entry
 * @entry: Ledger entry
 *
 * Return: NULL if entry has no notes, or a pointer to the notes.
 * Do not try to free() the return value.
 */
const char EXPORT *
egm_entry_get_notes(Egentry *entry)
{
        return entry->j_notes;
}

/**
 * egm_next_entry - Get the next ledger entry
 * @egm: Egmoney handle
 * @entry: Previous entry, or NULL to get the origin entry
 *
 * Return: Next entry, or %NULL if @entry is the final entry.
 * If both the return value and @entry are NULL, egm_errno will
 * be set to %EGM_INVAL; you most likely have not yet initialized
 * @egm yet.
 */
Egentry EXPORT *
egm_next_entry(Egmoney *egm, Egentry *entry)
{
        Egentry *next;
        if (!entry) {
                if (list_is_empty(&egm->entry_list))
                        return NULL;
                list_for_each_entry(next, &egm->entry_list, j_list) {
                        if (egm_in_range__(egm, next))
                                return next;
                }
                return NULL;
        } else {
                next = list_next_entry(entry, j_list);
                if (&next->j_list == &egm->entry_list)
                        return NULL;
                return next;
        }
}

/**
 * egm_new_entry - Create a new ledger entry
 * @date: Date of the new entry (transaction date, not the entry time stamp
 *
 * Return: Pointer to the new entry if successful, or NULL if unsuccessful,
 * in which case egm_errno will be set.
 *
 * Use this result as an argument to egm_entry_add_trans() and
 * egm_entry_add_notes() before appending it to the ledger with
 * egm_entry_append().
 */
Egentry EXPORT *
egm_new_entry(unsigned long date)
{
        Egentry *entry = egm_entry_create__();
        if (entry == NULL) {
                egm_errno = EGM_ERRNO;
                return NULL;
        }
        entry->j_date = date;
        return entry;
}

int EXPORT
egm_entry_append(Egmoney *egm, Egentry *entry)
{
        if (!egm_entry_valid(entry))
                return -1;

        entry->j_stamp = (unsigned long)time(NULL);
        list_add_tail(&entry->j_list, &egm->entry_list);
        return egm_acct_sync__(egm);
}
