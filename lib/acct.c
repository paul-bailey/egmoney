#include "egm_internal.h"
#include <eg-devel.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>


/* **********************************************************************
 *                           Search
 ***********************************************************************/

static struct acct_t *
acct_seek(Egmoney *egm, int id)
{
        struct acct_t *ret;
        list_for_each_entry(ret, &egm->acct_list, unordered) {
                if (ret->id == id)
                        return ret;
        }
        egm_errno = EGM_EACCT;
        return NULL;
}


/* **********************************************************************
 *                           Output
 ***********************************************************************/

static int
egm_acct_add_recursive(Egmoney *egm, struct acct_t *acct, float val)
{
        acct->sum += val;

        /* Recursively do this for all ancestors */
        acct = acct_parent(acct);
        if (acct != NULL) {
                return egm_acct_add_recursive(egm, acct, val);
        } else {
                return 0;
        }
}

static int
egm_acct_add(Egmoney *egm, int id, float val)
{
        struct acct_t *acct;
        int parent;

        acct = acct_seek(egm, id);
        if (!acct)
                return -1;
        return egm_acct_add_recursive(egm, acct, val);
}

/* **********************************************************************
 *              Library internal functions
 ***********************************************************************/

/*
 * "create" counterpart only needed in account_readdb.c, so declared
 * static there.
 */
void hidden__
egm_acct_destroy__(struct acct_t *a)
{
        struct acct_t *child, *tmp;
        list_for_each_entry_safe(child, tmp, &a->children, siblings) {
                egm_acct_destroy__(child);
        }
        list_del(&a->unordered);
        list_del(&a->siblings);
        if (a->name != NULL)
                free(a->name);
        if (a->notes != NULL)
                free(a->notes);
        free(a);
}

int hidden__
egm_acct_editable__(Egmoney *egm, int id)
{
        struct acct_t *a;
        if ((a = acct_seek(egm, id)) == NULL)
                return 0;
        if (!list_is_empty(&a->children)) {
                egm_errno = EGM_EACCT;
                return 0;
        }
        return 1;
}


/* **********************************************************************
 *              Account Public functions
 ***********************************************************************/

/**
 * egm_account_value - Get the balance of an account
 * @egm: Egmoney handle
 * @id: Account number
 *
 * Return: Balance of the account.  If @id is invalid, egm_errno
 * will be set to %EGM_EACCT.
 */
float EXPORT
egm_account_value(Egmoney *egm, int id)
{
        struct acct_t *acct = acct_seek(egm, id);
        if (!acct)
                return 0.0;
        return acct->sum;
}

/**
 * egm_account_get_child - Get child account number
 * @egm: Egmoney handle
 * @parent_acct: Account number of the parent account, or 0 to start with
 *  the trunk of the account tree.
 * @last_child: Result of previous call to this function, or 0 to get
 *  the first child
 *
 * Return: zero if no @parent_acct has no more children, or if it has none
 *  at all (a leaf account), -1 if either @parent_acct or @last_child are
 *  both nonzero and invalid (also resulting in egm_errno getting set), or
 *  the account number of the next child account.
 */
int EXPORT
egm_account_get_child(Egmoney *egm, int parent_acct, int last_child)
{
        struct list_head *parent_list;
        struct acct_t *a;

        a = acct_seek(egm, parent_acct);
        if (!a)
                return -1;

        parent_list = &a->children;
        if (list_is_empty(parent_list))
                return 0;

        if (last_child == 0) {
                a = list_last_entry(parent_list, struct acct_t, siblings);
        } else {
                a = acct_seek(egm, last_child);
                if (!a)
                        return -1;
                a = list_prev_entry(a, siblings);
                if (&a->siblings == parent_list)
                        return 0;
        }
        return a->id;
}

/**
 * egm_acctstr - Get the account name
 * @egm: Egmoney handle
 * @id: Account number
 *
 * Return: Name of the account, or NULL if account does not exist, in
 * which case egm_errno will be set to %EGM_EACCT.
 */
const char EXPORT *
egm_acctstr(Egmoney *egm, int id)
{
        struct acct_t *acct = acct_seek(egm, id);
        if (!acct)
                return NULL;
        return acct->name;
}

/**
 * egm_account_has_child - Determine if account branches
 * @egm: Egmoney handle
 * @id: Account number to check
 *
 * Return: true or false
 */
int EXPORT
egm_account_has_child(Egmoney *egm, int id)
{
        struct acct_t *a;
        if ((a = acct_seek(egm, id)) == NULL)
                return 0;
        return !list_is_empty(&a->children);
}

/**
 * egm_refresh_balances - Refresh all balances
 * @egm: Egmoney handle
 *
 * Return: zero if successful, -1 if there is something wrong with the
 * database, in which egm_errno will be set.
 *
 * Call this after a egm_set_range() call, so the balances will reflect
 * the new range.
 */
int EXPORT
egm_refresh_balances(Egmoney *egm)
{
        struct acct_t *acct;
        Egentry *entry;

        list_for_each_entry(acct, &egm->acct_list, unordered) {
                acct->sum = 0.0;
        }

        egm_foreach_entry(egm, entry) {
                struct journal_trans_t *trans;
                if (!egm_in_range__(egm, entry))
                        continue;
                trans_foreach(trans, entry) {
                        if (egm_acct_add(egm, trans->acct, trans->amt))
                                return -1; /* egm_errno already set */
                }
        }
        return 0;
}
