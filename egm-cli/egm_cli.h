#ifndef EGM_CLI_H
#define EGM_CLI_H

#include <egmoney.h>
#include <time.h>
#include <stdio.h>

struct egm_args_t {
        struct tm start, end;
        const char *dbpath;
        int verbose;
        enum egm_style_t {
                EGM_HTML = 0,
                EGM_CSV,
                EGM_JSON,
                EGM_TERM,
        } style;
        enum egm_range_t range;
        Egmoney *egm;
};

/* append.c */
extern int do_append(int argc, char **argv, struct egm_args_t *args);
extern void fail(void);

/* html.c */
#define FSHOWBAL 1
#define FSHOWNAV 2
extern int pracct_html(FILE *fp, Egmoney *egm, unsigned int flags, const char *refdir);
extern int prdb_html(FILE *fp, Egmoney *egm, unsigned int flags, const char *refdir);
extern void print_html_report(const char *dir, Egmoney *egm);

/* print.c */
extern char *moneystr(float amt, int sign);
extern char *egm_entry_datestr(Egentry *entry);
extern int egm_print_journal(Egmoney *egm, int style);
extern int egm_print_accounts(Egmoney *egm, int style, int show_balance);

#endif /* EGM_CLI_H */
