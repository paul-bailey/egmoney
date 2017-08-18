#include "egm_internal.h"
#include <egxml.h>
#include <time.h>
#include <string.h>

int hidden__
egm_verify_xml__(FILE *fp)
{
        struct xml_prologue_t prol;
        if (xml_get_prologue(fp, &prol) != 0)
                return -1;

        /* Don't accept UTF-nn */
        if (strncmp(prol.encoding, "ASCII", 5))
                return -1;

        return 0;
}

int hidden__
egm_in_range__(Egmoney *egm, Egentry *entry)
{
        time_t t = entry->j_date;
        struct tm *tcmp = localtime(&t);
        switch (egm->range) {
        case EGM_ALLTIME:
                return 1;
        case EGM_RANGE:
                if (tcmp->tm_year < egm->start.tm_year)
                        return 0;
                if (tcmp->tm_year > egm->end.tm_year)
                        return 0;
                if (tcmp->tm_mon < egm->start.tm_mon)
                        return 0;
                if (tcmp->tm_mon > egm->end.tm_mon)
                        return 0;
                if (tcmp->tm_mday < egm->start.tm_mday)
                        return 0;
                if (tcmp->tm_mday > egm->end.tm_mday)
                        return 0;
                return 1;
        case EGM_QUARTER:
                return tcmp->tm_year == egm->start.tm_year
                       && tcmp->tm_mon / 3 == egm->start.tm_mon / 3;
        case EGM_MONTH:
                return tcmp->tm_year == egm->start.tm_year
                       && tcmp->tm_mon == egm->start.tm_mon;
        case EGM_YEAR:
                return tcmp->tm_year == egm->start.tm_year;
        default:
                /* TODO: set egm_errno? */
                return 0;
        }
}
