#include "egm_cli.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#define MAXDATELEN 20

#ifndef arraylen
# define arraylen(a_) (sizeof(a_)/sizeof((a_)[0]))
#endif /* arraylen */

#define SGRAPH(s_)  "\e[" s_ "m"
#define SOFF SGRAPH("0")
#define SBOLD SGRAPH("1")
#define SREV SGRAPH("7")
#define SFG_(c_) SGRAPH("3" c_)
#define SBG_(c_) SGRAPH("4" c_)
#define SRED_   "1"
#define SGRN_   "2"
#define SYEL_   "3"
#define SBLU_   "4"
#define SMAG_   "5"
#define SCYN_   "6"
#define SWHT_   "7"

static char *scredit;
static char *sdebit;
static char *snotes;
static char *sname;
static char *sbracket;
static char *smeta;
static char *soff;
static char *sattr;
static char *sid;

static void
journal_pad(int n)
{
        while (n-- > 0)
                putchar(' ');
}

static void
journal_bracket(int c)
{
        printf("%s%c%s", sbracket, c, soff);
}

static void
init_colors(void)
{
        if (isatty(fileno(stdout))) {
                scredit  = SOFF SBOLD SFG_(SRED_);
                sdebit   = SOFF SBOLD;
                snotes   = SOFF SFG_(SCYN_);
                sname    = SOFF SFG_(SMAG_);
                sbracket = SOFF SFG_(SBLU_);
                smeta    = SOFF SBOLD;
                soff     = SOFF;
                sattr    = SOFF SFG_(SYEL_);
                sid      = SOFF SBOLD SFG_(SYEL_);
        } else {
                scredit  = "";
                sdebit   = "";
                snotes   = "";
                sname    = "";
                sbracket = "";
                smeta    = "";
                soff     = "";
                sattr    = "";
                sid      = "";
        }
}

/**
 * egm_entry_datestr - Get the date of an entry as a string
 * @entry: Ledger entry
 *
 * Return: Date of @entry as a string in mm/dd/yyyy format, or NULL
 * if the string cannot be allocated, in which egm_errno will be set
 * to EGM_ERRNO.  You should free this with a call to free().
 */
char *
egm_entry_datestr(Egentry *entry)
{
        time_t t = (time_t)egm_entry_date(entry);
        struct tm *tm = localtime(&t);
        char *time_buf = malloc(MAXDATELEN);
        if (!time_buf) {
                egm_errno = EGM_ERRNO;
        } else {
                strftime(time_buf, MAXDATELEN, "%m/%d/%Y", tm);
                time_buf[MAXDATELEN - 1] = '\0';
        }
        return time_buf;
}

/* free(return_val) when done with it */
char *
moneystr(float amt, int sign)
{
        char buf[40];
        if (amt < 0)
                amt = -amt;
        else
                sign = 0;
        setlocale(LC_ALL, "");
        snprintf(buf, sizeof(buf), "%s$%'.2f%s",
                sign ? "(" : "", amt, sign ? ")" : "");
        setlocale(LC_ALL, "C");
        return strdup(buf);
}

static void
journal_print_spacer(void)
{
        static const char *entry_spacer =
                "+-----------------------------------------------------------------------+";
        printf("%s%s%s\n", sbracket, entry_spacer, soff);
}

static void
journal_print_entry_header(void)
{
        journal_print_spacer();
        journal_bracket('|');
        printf(" %s%-10s ", smeta, "Date:");
        journal_bracket('|');
        printf(" %s%-20s ", smeta, "Account/notes:");
        journal_bracket('|');
        printf(" %s%-3s ", smeta, "PR:");
        journal_bracket('|');
        printf(" %s%12s ", smeta, "Debit:");
        journal_bracket('|');
        printf(" %s%12s ", smeta, "Credit:");
        journal_bracket('|');
        putchar('\n');
}

static void
journal_print_entry_footer(void)
{
        journal_print_spacer();
}
/*
 * journal_print_entry - Print journal entry as a traditional
 *                       accounting double-entry
 * @entry: Entry to print
 */
static void
journal_print_entry(Egmoney *egm, Egentry *entry)
{
        int i;
        char *s;
        struct egm_trans_t data;

        journal_print_spacer();

        i = 0;
        egm_foreach_trans(entry, &data) {
                s = moneystr(data.amt, 0);
                if (i > 0) {
                        journal_bracket('|');
                        journal_pad(12);
                        journal_bracket('|');
                } else {
                        journal_bracket('|');
                        printf(" %s%10s%s ",
                               smeta, egm_entry_datestr(entry), soff);
                        journal_bracket('|');
                }
                printf(" %s%-20s%s ", sname, egm_acctstr(egm, data.acct_no), soff);
                journal_bracket('|');
                printf(" %s%-3d%s ", sattr, data.acct_no, soff);
                journal_bracket('|');
                if (data.amt < 0.000) {
                        journal_pad(14);
                        journal_bracket('|');
                        printf(" %s%12s%s ", scredit, s, soff);
                } else {
                        printf(" %s%12s%s ", sdebit, s, soff);
                        journal_bracket('|');
                        journal_pad(14);
                }
                journal_bracket('|');
                putchar('\n');
                free(s);
                ++i;
        }
        s = (char *)egm_entry_get_notes(entry);
        if (s) {
                journal_bracket('|');
                printf(" %s%-69s%s ", snotes, s, soff);
                journal_bracket('|');
                putchar('\n');
        }
}

static void
json_print_attr(int margin, const char *attr, const char *id)
{
        journal_pad(margin);
        printf("%s\"%s\"%s: %s\"%s\"%s,\n",
               sattr, attr, soff, sid, id, soff);
}

static void
json_print_attr_array(int margin, const char *attr)
{
        journal_pad(margin);
        printf("%s\"%s\"%s: ", sattr, attr, soff);
        printf("%s{[%s\n", sbracket, soff);
}

static void
json_print_name(int margin, const char *name)
{
        journal_pad(margin);
        printf("%s\"%s\"%s: ", sname, name, soff);
        journal_bracket('{');
        putchar('\n');
}

static void
json_end_line(int margin, int bracket, int delim)
{
        journal_pad(margin);
        journal_bracket(bracket);
        if (delim)
                putchar(delim);
        putchar('\n');
}

static void
json_end_array(int margin)
{
        journal_pad(margin);
        printf("%s]}%s\n", sbracket, soff);

}

/*
 * journal_print_json - Print journal entry in the same format as the
 *                 ASCII .json file we read int
 * @entry: Entry to print
 */
static void
journal_print_json(Egmoney *egm, Egentry *entry)
{
        char *s;
        struct egm_trans_t data;

        json_print_name(0, "entry");
        json_print_attr(2, "date", egm_entry_datestr(entry));

        egm_foreach_trans(entry, &data) {
                /*
                 * For json, keep output parseable by printing
                 * non-locale version of "amt"
                 */
                char trid[12];
                char amt[50];
                snprintf(amt, sizeof(amt)-1, "%.2f", data.amt);
                snprintf(trid, sizeof(trid)-1, "%d", data.acct_no);
                trid[sizeof(trid)-1] = '\0';
                json_print_name(2, "transaction");
                json_print_attr(4, "account", trid);
                json_print_attr(4, "amount", amt);
                json_end_line(2, '}', ',');
        }

        s = (char *)egm_entry_get_notes(entry);
        if (s) {
                json_print_attr(2, "notes", s);
        }

        json_end_line(0, '}', 0);
}

/*
 * journal_print_csv_header - Print a header to accompany journal_print_csv()
 */
static void
journal_print_csv_header(void)
{
        static const char *titles[] = {
                "date",
                "account name",
                "account no",
                "credit",
                "debit",
                "notes"
        };
        int i;
        for (i = 0; i < arraylen(titles); i++)
                printf("\"%s\",", titles[i]);

        printf("\n");
}

/*
 * journal_print_csv - Print journal entry as a CSV version of a
 *                     traditional accounting double-entry
 * @entry: Entry to print
 */
static void
journal_print_csv(Egmoney *egm, Egentry *entry)
{
        int i;
        struct egm_trans_t data;

        i = 0;
        egm_foreach_trans(entry, &data) {
                char *s;
                s = moneystr(data.amt, 0);
                if (i == 0)
                        printf("\"%s\",", egm_entry_datestr(entry));
                else
                        printf(",");
                printf("\"%s\",\"%d\",",
                       egm_acctstr(egm, data.acct_no), data.acct_no);
                if (data.amt < 0.0)
                        printf(",\"%s\",", s);
                else
                        printf("\"%s\",,", s);

                free(s);

                if (i == 0) {
                        s = (char *)egm_entry_get_notes(entry);
                        if (s)
                                printf("\"%s\"", s);
                }
                printf("\n");
                i++;
        }
}

struct print_cb_t {
        void (*print_journal_header)(void);
        void (*print_journal_main)(Egmoney *, Egentry *);
        void (*print_journal_footer)(void);
};

static const struct print_cb_t print_cb_csv = {
        .print_journal_header = journal_print_csv_header,
        .print_journal_main = journal_print_csv,
        .print_journal_footer = NULL,
};

static const struct print_cb_t print_cb_json = {
        .print_journal_header = NULL,
        .print_journal_main = journal_print_json,
        .print_journal_footer = NULL,
};

static const struct print_cb_t print_cb_term = {
        .print_journal_header = journal_print_entry_header,
        .print_journal_main = journal_print_entry,
        .print_journal_footer = journal_print_entry_footer,
};

static void
pracct_term_r(Egmoney *egm, int acct, int depth, int show_balance)
{
        int i;
        int child = 0;
        const char *name;

        if (acct != 0) {
                name = egm_acctstr(egm, acct);
                for (i = 0; i < depth - 1; i++)
                        printf("    ");

                printf("%s%3d%s", sattr, acct, soff);
                putchar(' ');
                printf("%s%-16s%s", sname, name, soff);
                if (show_balance) {
                        float v = egm_account_value(egm, acct);
                        char *s = moneystr(v, 1);
                        putchar(' ');
                        printf("%sbal%s:", snotes, soff);
                        putchar(' ');
                        printf("%s%-12s%s",
                               v < 0.0 ? scredit : sdebit, s, soff);
                        free(s);
                }
                putchar('\n');
        }

        while ((child = egm_account_get_child(egm, acct, child)) != 0) {
                /* TODO: error */
                if (child < 0)
                        return;
                pracct_term_r(egm, child, depth + 1, show_balance);
        }
}

static void
pracct_csv_r(Egmoney *egm, int acct, int show_balance)
{
        int child = 0;
        const char *name;

        if (acct != 0) {
                name = egm_acctstr(egm, acct);

                printf("\"%d\",\"%s\"", acct, name);
                if (show_balance) {
                        float v = egm_account_value(egm, acct);
                        char *s = moneystr(v, 1);
                        printf(",\"%s\"", s);
                        free(s);
                }
                putchar('\n');
        }

        while ((child = egm_account_get_child(egm, acct, child)) != 0) {
                /* TODO: error */
                if (child < 0)
                        return;
                pracct_csv_r(egm, child, show_balance);
        }
}

static void
pracct_json_r(Egmoney *egm, int acct, int depth, int show_balance)
{
        int child = 0;
        int count = 0;

        char sacct[12];
        snprintf(sacct, sizeof(sacct), "%d", acct);
        sacct[sizeof(sacct) - 1] = '\0';

        json_print_name(depth, "account");
        json_print_attr(depth + 1, "id", sacct);
        json_print_attr(depth + 1, "name", egm_acctstr(egm, acct));
        if (show_balance) {
                float v = egm_account_value(egm, acct);
                char *s = moneystr(v, 1);
                json_print_attr(depth + 1, "balance", s);
                free(s);
        }

        while ((child = egm_account_get_child(egm, acct, child)) != 0) {
                /* TODO: error */
                if (child < 0)
                        return;
                if (count == 0)
                        json_print_attr_array(depth + 1, "children");
                pracct_json_r(egm, child, depth + 2, show_balance);
                ++count;
        }

        if (count)
                json_end_array(depth + 1);
        json_end_line(depth, '}', ',');
}

/**
 * egm_print_journal - Print the general journal
 * @egm: Egmoney handle
 * @style: An &enum egm_style_t style
 *
 * Return: zero if @style is valid, -1 if not.
 */
int
egm_print_journal(Egmoney *egm, int style)
{
        const struct print_cb_t *cb;
        Egentry *entry;
        init_colors();
        switch (style) {
        case EGM_CSV:
                cb = &print_cb_csv;
                break;
        case EGM_JSON:
                cb = &print_cb_json;
                break;
        case EGM_TERM:
                cb = &print_cb_term;
                break;
        case EGM_HTML:
                prdb_html(stdout, egm, 0, ".");
                return 0;
        default:
                return -1;
        }

        if (cb->print_journal_header != NULL)
                cb->print_journal_header();

        egm_foreach_entry(egm, entry) {
                if (!egm_entry_valid(entry))
                        return -1;
                cb->print_journal_main(egm, entry);
        }

        if (cb->print_journal_footer != NULL)
                cb->print_journal_footer();
        return 0;
}

/**
 * egm_print_accounts - Print the chart of accounts
 * @egm: Egmoney handle
 * @style: An enumerated output format.
 * @show_balance: zero to hide balances, nonzero (true) to show balances
 */
int
egm_print_accounts(Egmoney *egm, int style, int show_balance)
{
        init_colors();
        switch (style) {
        case EGM_CSV:
                pracct_csv_r(egm, 0, show_balance);
                break;
        case EGM_JSON:
                pracct_json_r(egm, 0, 0, show_balance);
                break;
        case EGM_TERM:
                pracct_term_r(egm, 0, 0, show_balance);
                break;
        case EGM_HTML:
                pracct_html(stdout, egm, show_balance ? FSHOWBAL : 0, ".");
                break;
        default:
                return -1;
        }
        return 0;
}
