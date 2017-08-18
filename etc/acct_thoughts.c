#include "jnl_common.h"
#include <eglist.h>

struct acct_t {
        struct list_head all_list;
        struct list_head type_list;
        int nr;
        char name[100];
        float sum;
        int type;
        int used:1;
};

struct token_t {
        char *s;
        int size;
        int len;
};

enum {
        TK_EOL,
        TK_EXPR,
        TK_COMM,
};

static void
token_free(struct token_t *t)
{
        if (t->s)
                free(t->s);
        memset(t, 0, sizeof(*t));
}

static void
token_putc(struct token_t *t, int c)
{
        if (t->len == token->size) {
                t->s = realloc(t->s, t->size + 512);
                t->size += 512;
                if (!t->s) {
                        perror("realloc failed");
                        exit(EXIT_FAILURE);
                }
        }

        t->s[t->len] = c;
        t->len++;
}

static void
token_unputc(struct token_t *t)
{
        if (t->len > 0)
                t->len = 0;
}

static int
token_empty(struct token_t *t)
{
        return t->s[0] == '\0';
}

#define DECLARE_LIST(name_) \
  static struct list_head name_##_list = LIST_HEAD_INIT(name_##_list)

DECLARE_LIST(acct_list);
DECLARE_LIST(asset_list);
DECLARE_LIST(liability_list);
DECLARE_LIST(equity_list);
DECLARE_LIST(revenue_list);
DECLARE_LIST(expense_list);
DECLARE_LIST(contra_list);


/* slide to end-of-comment (ie to end-of-line), don't allow escaping eol */
static void
eocslide(FILE *fp)
{
        int c;
        while ((c = getc(fp)) != EOF && c != '\n')
                ;
}

static const char *
gettok(const char *line, struct token_t *t)
{
        int c;
        int quot = 0;
        int quotc;
        int esc = 0;
        const char *s = line;

        t->len = 0;

        while (isblank(c = *s++))
                ;

        if (c == '\0') {
                --s;
                goto out;
        }

        do {
                switch (c) {
                case '\\':
                        if (esc || quoted) {
                                token_putc(t, c);
                                esc = 0;
                        } else {
                                esc = 1;
                        }
                        break;
                case '\'':
                case '"':
                        if (esc) {
                                token_putc(t, c);
                        } else if (!quoted) {
                                quotc = c;
                                quoted = 1;
                        } else if (quotc == c) {
                                quoted = 0;
                        }
                        break;
                case ' ':
                case '\t':
                        if (esc || quoted) {
                                token_putc(c);
                        } else {
                                goto out;
                        }
                        break;
                default:
                        if (esc) {
                                fprintf(stderr,
                                        "Warning: unkown escape '%c'\n",
                                        c);
                        }
                        token_putc(c);
                        break;
                }
                if (c != '\\')
                        esc = 0;
        } while ((c = *s++) != '\0');

        --s;
        if (quoted)
                fprintf(stderr, "Warning: Unterminated quote\n");

out:
        token_putc('\0');
        return s;
}

static int
parse_line(const char *line)
{
        static const struct a_t {
                int i;
                const char *s;
                struct list_head *list;
        } a[] = {
                { .i = JC_ASSET,   .s = "asset",     .list = &asset_list },
                { .i = JC_LIAB,    .s = "liability", .list = &liability_list },
                { .i = JC_EQUITY,  .s = "equity",    .list = &equity_list },
                { .i = JC_REVENUE, .s = "revenue",   .list = &revenue_list },
                { .i = JC_EXPENSE, .s = "expense",   .list = &expense_list },
                { .i = JC_CONTRA,  .s = "contra",    .list = &contra_list },
                { -1, NULL, NULL },
        }
        struct token_t t;
        struct acct_t *acct;
        char *s = line;
        int i;

        memset(&t, 0, sizeof(t));
        acct = malloc(sizeof(*acct));
        if (!acct)
                goto emalloc;

        s = gettok(&t, s);
        if (token_empty(&t))
                goto emptyline;

        acct->nr = strtoul(t.s, &endptr, 10);
        if (errno || endptr == t.s)
                goto esyntax;

        s = gettok(&t, s);
        if (token_empty(&t))
                goto esyntax;

        strncpy(acct->name, t.s, sizeof(acct->name));
        acct->name[sizeof(acct->name) - 1] = '\0';

        s = gettok(&t, s);
        if (token_empty(&t))
                goto esyntax;
        for (i = 0; a[i].s != NULL; i++) {
                if (!strcmp(t.s, a[i].s)) {
                        acct->type = a[i].i;
                        break;
                }
        }

        if (a[i].s == NULL)
                goto esyntax;

        /* Also make sure no excess tokens exist */
        s = gettok(&t, s);
        if (!token_empty(&t))
                goto eqyntax;

        acct->sum = 0.0;
        acct->used = 0;
        list_add(acct->all_list, &acct_list);
        list_add(acct->type_list, a[i].list);

emptyline:
        token_free(&t);
        return 0;

esyntax:
        free(acct);
emalloc:
        token_free(&t);
        return -1;
}

static void
agetline(FILE *fp, struct token_t *t)
{
        int c;
        t->len = 0;
        while ((c = getc(fp)) != EOF) {
                switch (c) {
                case ' ':
                        if (t->len != 0)
                                token_putc(t, c);
                        break;
                case '\\':
                        esc = !esc;
                        token_putc(t, c);
                        break;
                case '"':
                        if (!esc)
                                quot = !quot;
                        token_putc(t, c);
                        break;
                case '\n':
                        if (esc)
                                token_unputc(t);
                        else
                                goto eol;
                        break;
                case '#':
                        if (!(quot || esc)) {
                                eocslide(fp);
                                goto eol;
                        }
                        token_putc(c);
                        break;

                }
                if (c != '\\')
                        esc = 0;
        }
        token_putc(t, '\0');
}

int
acct_init__(void)
{
        FILE *fp = acct_open__();
        struct token_t t;
        if (!fp)
                return -1;

        memset(&t, 0, sizeof(t));
        do {
                agetline(fp, &t);
                if (!token_empty(&t)) {
                        if (parse_line(t.s))
                                return -1;
                }
        } while (!feof(fp));
        if (list_is_empty(&acct_list))
                return -1;
        return 0;
}
