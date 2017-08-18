#ifndef EGMONEY_H
#define EGMONEY_H

#include <stdio.h>

struct egentry_t;
typedef struct egentry_t Egentry;
struct egmoney_t;
typedef struct egmoney_t Egmoney;

/* Do not edit this macro! */
#define EGM_TRANSPERENTRY 8

enum egm_err_t {
        EGM_DBORIG = 1,
        EGM_DBFAIL,
        EGM_ERRNO,
        EGM_INVAL,
        EGM_EACCT,
        EGM_EFULL,
        EGM_ESYNC,
        EGM_EBACK,
        EGM_ESYNTAX,
};

enum egm_range_t {
        EGM_RANGE = 0,
        EGM_YEAR,
        EGM_QUARTER,
        EGM_MONTH,
        EGM_ALLTIME,
};

struct tm;

/*
 * For easy source-kelling, names of source files were kindly
 * added for these functions.
 */

/* err.c */
extern int egm_errno;
extern const char *egm_strerror(int err);
extern void egm_perror(const char *fmt, ...);
extern void egm_setlog(FILE *fp);

/* egm_struct.c */
extern Egmoney *egm_init(void);
extern void egm_exit(Egmoney *egm);
extern void egm_set_range(Egmoney *egm, int range,
                         struct tm *start, struct tm *end);
extern const char *egm_path(Egmoney *egm);

/* egmio.c */
extern int egm_readdb(Egmoney *egm, const char *path, int check_origin);



/* acct.c */
extern int egm_account_has_child(Egmoney *egm, int id);
extern const char *egm_acctstr(Egmoney *egm, int id);
extern float egm_account_value(Egmoney *egm, int id);
extern int egm_account_get_child(Egmoney *egm,
                                 int parent_acct, int last_child);
extern int egm_refresh_balances(Egmoney *egm);

/* journal.c */
extern int egm_entry_balanced(Egentry *entry);
extern int egm_entry_valid(Egentry *entry);
extern unsigned long egm_entry_stamp(Egentry *entry);
extern unsigned long egm_entry_date(Egentry *entry);
struct egm_trans_t {
        float amt;
        int acct_no;
};
extern int egm_entry_add_trans(Egmoney *egm, Egentry *entry,
                               const struct egm_trans_t *trans);
extern void *egm_entry_next_trans(Egentry *entry, void *last,
                                  struct egm_trans_t *trans);
#define egm_foreach_trans(entry_, tr_) \
        for (void *p_ = egm_entry_next_trans(entry_, NULL, tr_); \
             p_ != NULL; p_ = egm_entry_next_trans(entry_, p_, tr_))
extern void egm_entry_add_notes(Egentry *entry, const char *notes);
extern const char *egm_entry_get_notes(Egentry *entry);
extern Egentry *egm_next_entry(Egmoney *egm, Egentry *entry);
extern Egentry *egm_new_entry(unsigned long date);
extern int egm_entry_append(Egmoney *egm, Egentry *entry);
#define egm_foreach_entry(egm_, iter_) \
        for (iter_ = egm_next_entry(egm_, NULL); \
             iter_ != NULL; iter_ = egm_next_entry(egm_, iter_))


#endif /* EGMONEY_H */
