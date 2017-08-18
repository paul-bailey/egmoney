#include "egm_internal.h"
#include <eg-devel.h>
#include <egxml.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/param.h>

static void
egm_set_path(Egmoney *egm, const char *dir)
{
        strncpy(egm->path, dir, sizeof(egm->path));
        egm->path[sizeof(egm->path) - 1] = '\0';
}

static FILE *
egm_open_db(Egmoney *egm, const char *permissions)
{
        FILE *fp;
        if (eg_pushd(egm_path(egm)) < 0)
                return NULL;
        if (permissions[0] == 'w') {
                if (file_backup("db.bak.xml", "db.xml") < 0) {
                        eg_popd();
                        return NULL;
                }
        }
        fp = fopen("db.xml", permissions);
        if (eg_popd() < 0) {
                fclose(fp);
                return NULL;
        }
        return fp;
}

static int
print_egmoney_cb(FILE *fp, int state, void *priv)
{
        if (egm_account_writedb__(fp, state, priv) != 0)
                return -1;
        if (egm_journal_writedb__(fp, state, priv) != 0)
                return -1;
        return 0;
}

int hidden__
egm_acct_sync__(Egmoney *egm)
{
        int ret = -1;
        FILE *fp;
        struct xml_runner_t runner;

        fp = egm_open_db(egm, "w");
        if (!fp)
                return -1;

        fprintf(fp, "<?xml version=\"1.0\" encoding=\"ASCII\"?>\n");

        runner.flags = XML_NL;
        runner.priv  = egm;
        runner.cb = print_egmoney_cb;
        runner.tag = xml_tag_create("egmoney");
        if (!runner.tag)
                goto err;
        xml_tag_recursive(fp, 0, &runner);
        if (egm_errno)
                goto err;
        xml_tag_free(runner.tag);
        ret = 0;

err:
        fclose(fp);
        return 0;
}

static int
parse_egmoney_elem(FILE *fp, void *priv, XmlTag *tag)
{
        static const struct xml_elem_parser egmoney_elements[] = {
                { "account", egm_account_readdb__ },
                { "ledger",  egm_journal_readdb__ },
                { NULL, NULL },
        };
        return xml_parse_elements(fp, priv, egmoney_elements, "egmoney");
}

int EXPORT
egm_readdb(Egmoney *egm, const char *path, int check_origin)
{
        static const struct xml_elem_parser root_element = {
                .name = "egmoney",
                .parse = parse_egmoney_elem,
        };
        FILE *fp;

        if (path != NULL)
                egm_set_path(egm, path);

        fp = egm_open_db(egm, "r");
        if (!fp)
                goto enoclose;

        if (egm_verify_xml__(fp) < 0)
                goto eclose;
        if (xml_parse_elements(fp, egm, &root_element, NULL) < 0)
                goto eclose;
        fclose(fp);
        return egm_refresh_balances(egm);

eclose:
        /*
         * fp's position should not have changed after an error was
         * trapped, so we're waiting until now to report the error.
         */
        egm_syntax__(fp);
        fclose(fp);
enoclose:
        egm_set_errno(EGM_DBFAIL);
        return -1;
}
