#include "egm_cli.h"
#include <eg-devel.h>
#include <string.h>
#include <egxml.h>
#include <setjmp.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>

static void
print_header(FILE *fp, const char *title, const char *style_path)
{
        static const char *html_header_1 =
                "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
                "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
                "<head>\n"
                "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n";

        static const char *html_header_2 =
                "  <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js\"></script>\n"
                "</head>\n";

        fprintf(fp, "%s", html_header_1);
        fprintf(fp, "  <link rel = \"stylesheet\" type = \"text/css\" href = \"%s/common.css\" />\n",
                style_path);
        fprintf(fp, "  <title>%s</title>\n", title);
        fprintf(fp, "%s", html_header_2);
}

static void
html_bail_on(int cond, jmp_buf env)
{
        bail_on(cond, env, 1);
}

static XmlTag *
get_tag(const char *name, jmp_buf env)
{
        XmlTag *ret = xml_tag_create(name);
        html_bail_on(ret == NULL, env);
        return ret;
}

static void
add_attr(XmlTag *tag, const char *name,
         const char *value, jmp_buf env)
{
        int res = xml_add_attribute(tag, name, value);
        html_bail_on(res != 0, env);
}

static int
pr_html_td_cb(FILE *fp, int state, void *priv)
{
        fprintf(fp, "%s", (const char *)priv);
        return 0;
}

static void
pr_html_ep(FILE *fp, int state, const char *tag, const char *contents, const char **attrs, jmp_buf env)
{
        struct xml_runner_t runner = {
                .tag = get_tag(tag, env),
                .priv = (void *)contents,
                .flags = 0,
                .cb = pr_html_td_cb,
        };
        if (attrs != NULL) {
                int i;
                for (i = 0; attrs[i] != NULL; i += 2) {
                        html_bail_on(attrs[1] == NULL, env);
                        xml_add_attribute(runner.tag, attrs[i], attrs[i + 1]);
                }
        }
        xml_tag_recursive(fp, state, &runner);
        free(runner.tag);
}

static void
pr_html_td(FILE *fp, int state, const char *contents, const char **attrs, jmp_buf env)
{
        pr_html_ep(fp, state, "td", contents, attrs, env);
}

static void
pr_html_a(FILE *fp, int state, const char *contents, const char **attrs, jmp_buf env)
{
        pr_html_ep(fp, state, "a", contents, attrs, env);
}

static void
try_or_trap(jmp_buf env)
{
        if (setjmp(env) != 0) {
                egm_errno = EGM_ERRNO;
                fail();
        }
}


/* **********************************************************************
 *              NAV
 ***********************************************************************/

struct nav_vec {
        const char *href;
        const char *descr;
};

struct prnav_t {
        const struct nav_vec *vec;
        jmp_buf env;
};

static int
prnav_html_href(FILE *fp, int state, void *priv)
{
        struct prnav_t *a = priv;
        const char *attrs[] = { "href", a->vec->href, NULL };
        pr_html_a(fp, state, a->vec->descr, attrs, a->env);
        return 0;
}

static int
prnav_html_ul(FILE *fp, int state, void *priv)
{
        struct prnav_t *a = priv;
        struct xml_runner_t runner = {
                .priv = a,
                .flags = XML_NL,
                .cb = prnav_html_href,
                .tag = get_tag("li", a->env),
        };
        while (a->vec->href != NULL) {
                xml_tag_recursive(fp, state, &runner);
                a->vec++;
        }
        xml_tag_free(runner.tag);
        return 0;
}

static void
prnav_html(FILE *fp, struct prnav_t *a, int state)
{
        struct xml_runner_t runner = {
                .priv = a,
                .flags = XML_NL,
                .cb = prnav_html_ul,
                .tag = get_tag("ul", a->env),
        };
        add_attr(runner.tag, "id", "nav", a->env);
        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
}

/* **********************************************************************
 *              Print COA
 ***********************************************************************/

struct prhtml_t {
        Egmoney *egm;
        unsigned int flags;
        int acct;
        jmp_buf env;
        int depth;
};

#define SHOW_BALANCE(p_) (!!((p_)->flags & FSHOWBAL))
#define SHOW_NAV(p_)     (!!((p_)->flags & FSHOWNAV))

static int
pracct_html_hdr_tr(FILE *fp, int state, void *priv)
{
        const char *tds[] = { "Account", "PR", "Balance" };
        struct prhtml_t *a = priv;
        int n, i;

        n = SHOW_BALANCE(a) ? 3 : 2;
        for (i = 0; i < n; ++i) {
                pr_html_td(fp, state, tds[i], NULL, a->env);
        }
        return 0;
}

static void
pracct_html_hdr(FILE *fp, struct prhtml_t *a, int state)
{
        struct xml_runner_t runner = {
                .tag = get_tag("tr", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = pracct_html_hdr_tr,
        };
        add_attr(runner.tag, "id", "hdr", a->env);
        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
}

static int
pracct_html_tr(FILE *fp, int state, void *priv)
{
        char buf[40];
        struct prhtml_t *a = priv;
        pr_html_td(fp, state, egm_acctstr(a->egm, a->acct), NULL, a->env);

        snprintf(buf, sizeof(buf), "%d", a->acct);
        buf[sizeof(buf) - 1] = '\0';

        pr_html_td(fp, state, buf, NULL, a->env);

        if (SHOW_BALANCE(a)) {
                char *bal = moneystr(egm_account_value(a->egm, a->acct), 1);
                pr_html_td(fp, state, bal, NULL, a->env);
                free(bal);
        }
        return 0;
}

static void
pracct_html_table_r(FILE *fp, struct prhtml_t *a, int state)
{
        int child = 0;
        struct xml_runner_t runner = {
                .tag = get_tag("tr", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = pracct_html_tr,
        };

        if (egm_account_has_child(a->egm, a->acct)) {
                char buf[20];
                snprintf(buf, sizeof(buf), "depth%d", a->depth);
                buf[sizeof(buf)-1] = '\0';
                add_attr(runner.tag, "class", buf, a->env);
        }

        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);

        while ((child = egm_account_get_child(a->egm, a->acct, child)) != 0) {
                struct prhtml_t *b = malloc(sizeof(*b));
                html_bail_on(child < 0 || b == NULL, a->env);
                memcpy(b, a, sizeof(*b));
                b->acct = child;
                b->depth++;
                pracct_html_table_r(fp, b, state);
                free(b);
        }
}

static int
pracct_html_table(FILE *fp, int state, void *priv)
{
        struct prhtml_t *a = priv;
        a->acct = 0;
        a->depth = -1;
        pracct_html_hdr(fp, a, state);
        pracct_html_table_r(fp, a, state);
        return 0;
}

static int
pracct_html_body(FILE *fp, int state, void *priv)
{
        static const struct nav_vec bal_nav[] = {
                { "./coa.html", "Chart of Accounts" },
                { "./balance.html", "Balance Sheet" },
                { "./ledger.html", "Ledger" },
                { NULL, NULL }
        };
        static const struct nav_vec coa_nav[] = {
                { "./coa.html", "Chart of Accounts" },
                { "./balance.html", "Balance Sheet" },
                { "./ledger.html", "Ledger" },
                { NULL, NULL }
        };
        struct prhtml_t *a = priv;
        struct xml_runner_t runner = {
                .tag = get_tag("table", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = pracct_html_table,
        };

        if (SHOW_NAV(a)) {
                struct prnav_t nav = {
                        .vec = (SHOW_BALANCE(a) ? bal_nav : coa_nav),
                };
                memcpy(&nav.env, &a->env, sizeof(nav.env));
                prnav_html(fp, &nav, state);
        }

        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
        return 0;
}

int
pracct_html(FILE *fp, Egmoney *egm, unsigned int flags, const char *refdir)
{
        const char *title;
        struct prhtml_t a = {
                .egm = egm,
                .flags = flags,
        };
        struct xml_runner_t runner;
        try_or_trap(a.env);
        if (SHOW_BALANCE(&a))
                title = "EGMoney Balance Sheet";
        else
                title = "EGMoney Chart of Accounts";
        print_header(fp, title, refdir);
        runner.tag = get_tag("body", a.env);
        runner.priv = &a;
        runner.flags = XML_NL;
        runner.cb = pracct_html_body;
        xml_tag_recursive(fp, 0, &runner);
        xml_tag_free(runner.tag);
        return 0;
}

/* **********************************************************************
 *              Print DB
 ***********************************************************************/

struct prdb_t {
        Egmoney *egm;
        Egentry *entry;
        int entryi;
        int nr;
        unsigned int flags;
        float amt;
        jmp_buf env;
};

#define DBCOLSPERTBL 5

static int
prdb_html_tr_hdr(FILE *fp, int state, void *priv)
{
        static const char *vec[DBCOLSPERTBL] = {
                "Date", "Account", "PR",
                "Debit", "Credit"
        };
        struct prdb_t *a = priv;
        int i;

        for (i = 0; i < DBCOLSPERTBL; i++) {
                pr_html_td(fp, state, vec[i], NULL, a->env);
        }
        return 0;
}

static void
prdb_html_table_hdr(FILE *fp, struct prdb_t *a, int state)
{
        struct xml_runner_t runner = {
                .tag = get_tag("tr", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = prdb_html_tr_hdr,
        };
        add_attr(runner.tag, "id", "hdr", a->env);
        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
}

static int
prdb_html_tr(FILE *fp, int state, void *priv)
{
        char *vec[DBCOLSPERTBL];
        struct prdb_t *a;
        int i;
        char acct[20];
        char *sval;
        char *date = NULL;

        a = priv;

        snprintf(acct, sizeof(acct), "%d", a->nr);
        acct[sizeof(acct) - 1] = '\0';
        sval = moneystr(a->amt, 0);
        if (!a->entryi)
                date = (char *)egm_entry_datestr(a->entry);

        vec[0] = date ? date : "";
        vec[1] = (char *)egm_acctstr(a->egm, a->nr);
        vec[2] = acct;
        if (a->amt < 0.0) {
                vec[3] = "";
                vec[4] = sval;
        } else {
                vec[3] = sval;
                vec[4] = "";
        }

        for (i = 0; i < DBCOLSPERTBL; i++)
                pr_html_td(fp, state, vec[i], NULL, a->env);

        free(sval);
        if (date != NULL)
                free(date);
        return 0;
}

static int
prdb_html_notes(FILE *fp, int state, void *priv)
{
        static const char *attrs[] = {
                "colspan", "5", "class", "notes", NULL
        };
        struct prdb_t *a = priv;
        const char *notes = egm_entry_get_notes(a->entry);
        /* even if it's nothing, we want a row of this class */
        if (!notes)
                notes = "";
        pr_html_td(fp, state, notes, attrs, a->env);
        return 0;
}

static int
prdb_html_entry(FILE *fp, int state, void *priv)
{
        struct prdb_t *a = priv;
        struct xml_runner_t runner = {
                .tag = get_tag("tr", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = prdb_html_tr,
        };
        int i;
        struct egm_trans_t data;

        i = 0;
        egm_foreach_trans(a->entry, &data) {
                a->entryi = i;
                a->nr = data.acct_no;
                a->amt = data.amt;
                xml_tag_recursive(fp, state, &runner);
                i++;
        }
        runner.cb = prdb_html_notes;
        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
        return 0;
}

static int
prdb_html_table(FILE *fp, int state, void *priv)
{
        struct prdb_t *a = priv;
        struct xml_runner_t runner = {
                .tag = get_tag("tr", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = prdb_html_entry,
        };
        Egentry *entry;

        prdb_html_table_hdr(fp, a, state);
        egm_foreach_entry(a->egm, entry) {
                a->entry = entry;
                xml_tag_recursive(fp, state, &runner);
        }
        xml_tag_free(runner.tag);
        return 0;
}

/*
 * TODO: All this stuff
 */
static int
prdb_html_body(FILE *fp, int state, void *priv)
{
        static const struct nav_vec acct_nav[] = {
                { "./coa.html", "Chart of Accounts" },
                { "./balance.html", "Balance Sheet" },
                { "./ledger.html", "Ledger" },
                { NULL, NULL }
        };
        struct prdb_t *a = priv;
        struct xml_runner_t runner = {
                .tag = get_tag("table", a->env),
                .priv = a,
                .flags = XML_NL,
                .cb = prdb_html_table,
        };

        if (SHOW_NAV(a)) {
                struct prnav_t nav = {
                        .vec = acct_nav,
                };
                memcpy(&nav.env, &a->env, sizeof(nav.env));
                prnav_html(fp, &nav, state);
        }

        xml_tag_recursive(fp, state, &runner);
        xml_tag_free(runner.tag);
        return 0;
}

int
prdb_html(FILE *fp, Egmoney *egm, unsigned int flags, const char *refdir)
{
        struct prdb_t a = {
                .egm = egm,
                .flags = flags,
        };
        struct xml_runner_t runner;
        try_or_trap(a.env);
        print_header(fp, "EGMoney General Ledger Journal", refdir);

        runner.tag = get_tag("body", a.env);
        runner.priv = &a;
        runner.flags = XML_NL;
        runner.cb = prdb_html_body;

        xml_tag_recursive(fp, 0, &runner);
        xml_tag_free(runner.tag);
        return 0;
}

/* **********************************************************************
 *                      Full dir splash
 ***********************************************************************/

static void
copy_stylesheet(void)
{
        /*
         * Just return quietly if error.  This file isn't that
         * important.
         */
        FILE *src, *dst;
        int c;
        src = fopen(DATADIR "/common.css", "r");
        if (!src)
                return;
        dst = fopen("./common.css", "w");
        if (!dst)
                goto edst;
        while ((c = getc(src)) != EOF)
                putc(c, dst);
        fclose(dst);
edst:
        fclose(src);
}

static void
print_html_report_indir(Egmoney *egm)
{
        FILE *fp;

        fp = fopen("balance.html", "w");
        if (!fp)
                goto err;
        if (pracct_html(fp, egm, FSHOWBAL | FSHOWNAV, ".") < 0)
                goto err;
        fclose (fp);

        fp = fopen("coa.html", "w");
        if (!fp)
                goto err;
        if (pracct_html(fp, egm, FSHOWNAV, ".") < 0)
                goto err;
        fclose(fp);

        fp = fopen("ledger.html", "w");
        if (!fp)
                goto err;
        if (prdb_html(fp, egm, FSHOWNAV, ".") < 0)
                goto err;

        copy_stylesheet();
        return;

err:
        egm_errno = EGM_ERRNO;
        fail();
}

void
print_html_report(const char *dir, Egmoney *egm)
{
        struct stat st;
        int res;
        if ((res = stat(dir, &st)) == 0 || errno != ENOENT)
                goto festat;
        /* errno was set above, reset it now */
        errno = 0;

        if (mkdir(dir, 0777) < 0)
                goto ferrno;

        if (eg_pushd(dir) < 0)
                goto ferrno;

        print_html_report_indir(egm);
        if (eg_popd() < 0)
                goto ferrno;

        return;

ferrno:
        egm_errno = EGM_ERRNO;
        fail();
        return;

festat:
        errno = 0;
        fprintf(stderr, "'%s' already exists\n", dir);
        egm_errno = EGM_INVAL;
        fail();
}
