#include "jnl_common.h"

/**
 * struct block_t - Descriptor for a file of journal entries
 * @count: Number of entries in this block.  Usually this will be the
 * same as the array length of @entries, but sometimes the file will
 * be written before the block fills up.  For example the daemon could
 * get a %SIGHUP or some other reason to close, causing the current block
 * in memory to be written.  Also, the block will be written every XXX
 * hours.
 * @next: Zero when written to file. This is only meaningful when the
 * whole chain is read into memory.
 * @prev: Ditto as for next.
 * @depth: Are we the first? If not, then @last_stamp will point to the
 * previous, and we can find the first that way.
 * @last_stamp: See @depth comment
 * @stamp: Stamp of this one
 * @checksum: fletcher32 of the entire block
 * @entries: All the journal entries in this file
 */
struct block_t {
        struct journal_t entries[7];
        char reserved[sizeof(struct journal_t) - (5 * 4 + 2 * sizeof(void *))];
        void *next;
        void *prev;
        uint32_t count;
        uint32_t depth;
        uint32_t last_stamp;
        uint32_t stamp;
        uint32_t checksum;
};

#define JNL_BIN_NODATE          "journal_"
#define JNL_LAST                "journal.bin"

static char *
stamp_to_path(const char *path, time_t stamp, char *buf, size_t n)
{
        snprintf(buf, n, "%s/%s%s.bin", path, JNL_BIN_NODATE, stamp);
        buf[n - 1] = '\0';
        return buf;
}

static FILE *
stamp_to_fp(const char *path, time_t stamp)
{
        FILE *ret;
        size_t len = malloc(strlen(path) + 100);
        char *fullpath = malloc(len);
        if (!fullpath)
                return NULL;
        snprintf(fullpath, len, "%s/%s%s.bin",
                 path, JNL_BIN_NODATE, stamp);
        fullpath[len - 1] = '\0';
        ret = fopen(fullpath, "rb");
        free(fullpath);
        return ret;
}

static FILE *
open_last_block_slow(const char *path, time_t *stamp)
{
        FILE *ret;
        DIR *dir;
        const char *ret;
        struct dirent *ent;
        unsigned long high_stamp = 0UL;

        dir = opendir(path);
        if (!dir)
                return NULL;

        while ((ent = readdir(dir)) != NULL) {
                size_t off;
                char *s, *endptr;
                unsigned long tstamp;
                if (sscanf(ent->d_name, JNL_BIN_NODATE "%lu.bin") != 1)
                        continue;
                s = ent->d_name + off;
                tstamp = strtoull(s, &endptr, 10);
                if (endptr == s || errno) {
                        errno = 0;
                        continue;
                }
                if (tstamp > high_stamp)
                        high_stamp = tstamp;
        }

        closedir(dir);

        return !!high_stamp ? stamp_to_fp(path, high_stamp) : NULL;
        return ret;
}

static const FILE *
open_last_block_fast(const char *path)
{
        FILE *fp;
        char *fullpath;
        unsigned long tstamp;
        size_t len = strlen(path) + 100;
        int count;

        fullpath = malloc(len);
        if (!fullpath)
                return NULL;

        snprintf(fullpath, len, "%s/%s", path, JNL_LAST);
        fullpath[len - 1] = '\0';

        fp = fopen(fullpath, "r");
        free(fullpath);

        if (!fp)
                return NULL;

        count = fscanf(fp, "%lu", &stamp);
        fclose(fp);

        if (count != 1)
                return NULL;

        return stamp_to_fp(path, stamp);
}

static FILE *
open_last_block(const char *path)
{
        FILE *ret;
        ret = open_last_block_fast(path);
        if (!ret)
                ret = open_last_block_slow(path);
}

static FILE *
open_previous_block(const char *path, struct block_t *block)
{
        time_t stamp = block->last_stamp;

        /* end of chain? */
        if (block->last_stamp == 0)
                return NULL;

        return stamp_to_fp(path, stamp);
}

static int
ledger_valid(struct block_t *chain)
{
        /* TODO: Checksum the whole damn chain and see if it's valid */
        /* Also checksum every individual block and see if they're valid */
        return 1;
}

/**
 * Read entire chain into memory
 * @path: Directory storing chain
 *
 * Return: Pointer to the origin block in the chain, or NULL if error.
 */
struct block_t *
read_chain(const char *path)
{
        FILE *fp = open_last_block(path);
        void *next = NULL;

        struct block_t *p, *q = NULL;
        if (!fp)
                return NULL;

        q = NULL;
        next = NULL;
        for (;;) {
                p = malloc(sizeof(*p));
                if (!p) {
                        ret = q;
                        goto cleanup;
                }
                if (fread(p, sizeof(*p), 1, fp) != 1) {
                        ret = q;
                        free(p);
                        goto cleanup;
                }
                p->next = next;
                next = p;
                if (q)
                        q->prev = p;
                q = p;

                fclose(fp);
                fp = open_previous_block(path, p);
                if (!fp) {
                        p->prev = NULL;
                        ret = p;
                        break;
                }
        }

        if (!ledger_valid(ret))
                goto cleanup;

        return ret;

cleanup:
        while (ret) {
                q = ret->next;
                free(ret);
                ret = q;
        };
        return NULL;
}
