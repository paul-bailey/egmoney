#include "egm_internal.h"
#include <stdlib.h>
#include <string.h>

Egmoney EXPORT *
egm_init(void)
{
        Egmoney *ret = malloc(sizeof(*ret));
        if (!ret) {
                egm_errno = EGM_ERRNO;
                return NULL;
        }

        init_list_head(&ret->entry_list);
        init_list_head(&ret->acct_list);
        ret->master_acct = NULL;
        ret->path[0] = '\0';
        ret->range = EGM_ALLTIME;
        return ret;
}

/**
 * egm_exit - Opposite of egm_init()
 * @egm: Egmoney handle to clean and free
 */
void EXPORT
egm_exit(Egmoney *egm)
{
        egm_journal_cleandb__(egm);
        egm_acct_destroy__(egm->master_acct);
        free(egm);
}

/**
 * egm_set_range - Set a range to filter transactions for printing,
 *         getting balances, et al.
 * @egm: Egmoney handle
 * @range: An &enum egm_range_t range
 * @start: tm struct describing the start range
 * @end: Currently unused; will be used when ready to handle %EGM_RANGE
 *   range.
 *
 * Note: Call this *before* a call to egm_readdb() if you want the
 * balances to fall within range.
 */
void EXPORT
egm_set_range(Egmoney *egm, int range, struct tm *start, struct tm *end)
{
        memcpy(&egm->start, start, sizeof(egm->start));
        memcpy(&egm->end, end, sizeof(egm->end));
        egm->range = range;
}

/**
 * egm_path - Get the path to the default database
 * @egm: Egmoney handle
 *
 * Return: Path to the default database.  This does not change
 * throughout the course of the program, so you only need to
 * call it once for a given @egm.
 */
const char EXPORT *
egm_path(Egmoney *egm)
{
        if (egm->path[0] == '\0') {
                snprintf(egm->path, sizeof(egm->path),
                         "%s/.egmoney", getenv("HOME"));
                egm->path[sizeof(egm->path) - 1] = '\0';
        }
        return egm->path;
}
