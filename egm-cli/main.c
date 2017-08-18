#include "egm_cli.h"
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>

static char *progname;

static void
usage(FILE *fp)
{
#if 1
        fprintf(fp, "TODO: Write this help\n");
#else
        static const char *options =
                "Options:\n"
                "        -a      all accounts; ACCT_NUMBER arg is not needed.\n"
                "        -v      more verbose; implies -a\n"
                "        -?      show this help and exit\n"
                "        -M      Show month-to-date\n"
                "        -Q      Show quarter-to-date\n"
                "        -Y      Show year-to-date\n";
        fprintf(fp,
                "%s - command line tool for egmoney\n"
                "Usage: %s [OPTIONS] SUBCOMMAND [ARGS...]\n"
                "%s\n",
                progname, progname, options);
#endif
}

static void
fill_today(struct tm *dst)
{
        time_t today;
        struct tm *tm;
        today = time(NULL);
        tm = localtime(&today);
        memcpy(dst, tm, sizeof(*dst));
}

static int
parse_args(int argc, char **argv, struct egm_args_t *args)
{
        int opt;
        args->verbose = false;
        args->range = EGM_ALLTIME;
        args->style = EGM_TERM;
        args->egm = NULL;
        args->dbpath = NULL;
        while ((opt = getopt(argc, argv, "cd:hjv?MQYs")) != -1) {
                switch (opt) {
                case 'c':
                        args->style = EGM_CSV;
                        break;
                case 'd':
                        args->dbpath = optarg;
                        break;
                case 'h':
                        args->style = EGM_HTML;
                        break;
                case 'j':
                        args->style = EGM_JSON;
                        break;
                case 'v':
                        args->verbose = true;
                        break;
                case '?':
                        usage(stdout);
                        return EXIT_SUCCESS;
                case 'M':
                        fill_today(&args->start);
                        args->range = EGM_MONTH;
                        break;
                case 'Q':
                        fill_today(&args->start);
                        args->range = EGM_QUARTER;
                        break;
                case 'Y':
                        fill_today(&args->start);
                        args->range = EGM_YEAR;
                        break;
                /*
                 * TODO: add the following options:
                 * -r mm/dd/yy:mm/dd/yy (range)
                 * -q q/yy      (quarter)
                 * -m mm/yy     (month)
                 * -y yy        (year)
                 */
                default:
                        usage(stderr);
                        return EXIT_FAILURE;
                }
        }
        return EXIT_SUCCESS;
}

static int
do_coa(int argc, char **argv, struct egm_args_t *args)
{
        egm_print_accounts(args->egm, args->style, 0);
        return EXIT_SUCCESS;
}

static int
do_dump(int argc, char **argv, struct egm_args_t *args)
{
        if (egm_print_journal(args->egm, args->style) < 0)
                fail();
        return EXIT_SUCCESS;
}

static int
do_path(int argc, char **argv, struct egm_args_t *args)
{
        printf("%s\n", egm_path(args->egm));
        return EXIT_SUCCESS;
}

static int
do_diff(int argc, char **argv, struct egm_args_t *args)
{
        char *endptr;
        int nr;
        float cmp, bal;
        if (argc - optind < 2) {
                fprintf(stderr, "Expected: ACCT VALUE\n");
                return EXIT_FAILURE;
        }
        nr = strtoul(argv[optind], &endptr, 10);
        if (endptr == argv[optind] || errno) {
                fprintf(stderr, "Invalid account '%s'\n", argv[optind]);
                return EXIT_FAILURE;
        }
        ++optind;
        cmp = strtof(argv[optind], &endptr);
        if (endptr == argv[optind] || errno) {
                fprintf(stderr, "Invalid amount '%s'\n", argv[optind]);
                return EXIT_FAILURE;
        }

        bal = egm_account_value(args->egm, nr);
        if (egm_errno)
                fail();
        printf("%.2f\n", bal - cmp);
        return EXIT_SUCCESS;
}

static int
do_balance(int argc, char **argv, struct egm_args_t *args)
{
        int acct;
        if (optind >= argc) {
                egm_print_accounts(args->egm, args->style, 1);
        } else {
                char *endptr;
                float v;
                acct = strtoul(argv[optind], &endptr, 10);
                if (endptr == argv[optind] || errno) {
                        egm_errno = EGM_INVAL;
                        fail();
                }
                v = egm_account_value(args->egm, acct);
                if (egm_errno)
                        fail();
                printf("%.2f\n", v);
        }
        return EXIT_SUCCESS;
}

static int
do_html(int argc, char **argv, struct egm_args_t *args)
{
        if (optind >= argc) {
                fprintf(stderr, "Expected: output directory\n");
                return EXIT_FAILURE;
        }
        printf("dest=%s\n", argv[optind]);
        print_html_report(argv[optind], args->egm);
        fprintf(stderr, "Success\n");
        return EXIT_SUCCESS;
}

/*
 * Some number that no platform will set EXIT_{SUCCESS|FAILURE}
 * to.
 */
static int
execute(int argc, char **argv, struct egm_args_t *args)
{
        static const struct {
                const char *name;
                int (*fn)(int, char **, struct egm_args_t *);
                int need_db;
        } subcs[] = {
                { "append",      do_append,     1 },
                { "balance",     do_balance,    1 },
                { "coa",         do_coa,        1 },
                { "dump",        do_dump,       1 },
                { "diff",        do_diff,       1 },
                { "path",        do_path,       0 },
                { "html",        do_html,       1 },
                { NULL, NULL },
        };
        int i;
        char *subcommand = argv[optind];
        ++optind;
        for (i = 0; subcs[i].name != NULL; ++i) {
                if (!strcmp(subcs[i].name, subcommand)) {
                        if (subcs[i].need_db) {
                                int res = egm_readdb(args->egm, args->dbpath, 1);
                                if (res < 0)
                                        fail();
                        }
                        return subcs[i].fn(argc, argv, args);
                }
        }
        fprintf(stderr, "Subcommand '%s' not found\n", subcommand);
        return EXIT_FAILURE;
}

static jmp_buf prog_jmp_buf;

void
fail(void)
{
        longjmp(prog_jmp_buf, egm_errno ? egm_errno : 10);
}

int
main(int argc, char **argv)
{
        struct egm_args_t args;

        progname = argv[0];
        if (setjmp(prog_jmp_buf)) {
                egm_perror("%s: ERROR:", progname);
                return EXIT_FAILURE;
        }

        parse_args(argc, argv, &args);
        if (optind >= argc) {
                fprintf(stderr, "Expected: subcommand\n");
                return EXIT_FAILURE;
        }

        args.egm = egm_init();
        if (!args.egm)
                fail();

        if (args.range != EGM_ALLTIME)
                egm_set_range(args.egm, args.range, &args.start, &args.end);

        return execute(argc, argv, &args);
}
