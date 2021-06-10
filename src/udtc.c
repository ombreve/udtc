#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "docs.h"

#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include "optparse.h"

#define UTF8_IMPLEMENTATION
#include "utf8.h"

#define READALL_CHUNK (16*1024)

static const char out_of_memory[] = "out of memory";

static FILE *cleanup_fd = 0;
static char *cleanup_file = 0;

/* Print a message and exit the program with a failure code. */
static void
fatal(const char *fmt, ...)
{
    va_list ap;

    if (cleanup_fd == stdout)
        putchar('\n'); /* restore a decent prompt */
    else if (cleanup_fd)
       fclose(cleanup_fd);
    if (cleanup_file)
       remove(cleanup_file);

    va_start(ap, fmt);
    fprintf(stderr, "udtc: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);

    exit(EXIT_FAILURE);
}

/* Print a non-fatal warning message. */
static void
warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "warning: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* Fallback method to get a password from terminal. */
static void
get_key_dumb(char *buf, size_t len, char *prompt)
{
    size_t passlen;
    warning("reading key from stdin with echo");
    fputs(prompt, stderr);
    fflush(stderr);
    if (!fgets(buf, len, stdin))
        fatal("could not read passphrase");
    passlen = strlen(buf);
    if (buf[passlen - 1] < ' ')
        buf[passlen - 1] = 0;
}

/* Read a password from terminal. */
static void
get_key(char *buf, size_t len, char *prompt)
{
    int tty;
    char newline = '\n';
    size_t i;
    struct termios old, new;

    tty = open("/dev/tty", O_RDWR);
    if (tty == -1)
        get_key_dumb(buf, len, prompt);
    else {
        if (write(tty, prompt, strlen(prompt)) == -1)
            fatal("error asking for key");
        tcgetattr(tty, &old);
        new = old;
        new.c_lflag &= ~ECHO;
        tcsetattr(tty, TCSANOW, &new);
        errno = 0;
        for (i = 0; i < len - 1 && read(tty, buf + i, 1) == 1; i++)
            if (buf[i] == '\n' || buf[i] == '\r')
                break;
        buf[i] = 0;
        tcsetattr(tty, TCSANOW, &old);
        if (write(tty, &newline, 1) == -1)
            fatal("error asking for passphrase");
        close(tty);
        if (errno)
            fatal("could not read key from /dev/tty");
    }
}

/* Read all the utf8 stream IN. Update LEN the number of read UTF code points
 * and return a pointer on an allocated code points buffer. */
static uint32_t*
read_all(FILE *in, size_t *len)
{
    uint32_t *udata = NULL;
    size_t size = 0, used = 0;
    uint32_t state = 0;
    int c;

    while((c = fgetc(in)) != EOF) {
        if (used + 1 > size) {
            size = used + READALL_CHUNK;
            if (!(udata = realloc(udata, sizeof(uint32_t)*size)))
                fatal(out_of_memory);
        }
        if (utf8_decode(&state, udata + used, (uint8_t)c) == UTF8_ACCEPT)
            used++;
        else if (state == UTF8_REJECT)
            fatal("bad input utf8 sequence");
    }
    if (ferror(in))
        fatal("read error");
    if (state != UTF8_ACCEPT)
        fatal("bad input utf8 last sequence");
    *len = used;
    return udata;
}

/* Write LEN utf code points from buffer UDATA to stream OUT. */
static void
write_all(FILE *out, uint32_t *udata, size_t len)
{
    uint8_t data[5], *p;
    uint32_t state;
    int i;

    for (i = 0; i < len; i++) {
        p = utf8_code(&state, data, udata[i]);
        if (state != UTF8_ACCEPT)
            fatal("bad output utf8 sequence");
        if (p >= data + sizeof(data))
            fatal("error in utf8 coding");
        *p = 0;
        if (fputs((char *)data, out) < 0)
            fatal("could not write output -- %s", strerror(errno));
    }
}

typedef struct {
    uint32_t code;
    int key;
    int subkey;
} tc_item;

/* Compare two TC_ITEM elements. */
static int
compare(const void *a, const void *b)
{
    if (((tc_item *) a)->key == ((tc_item *) b)->key)
        return ((tc_item *) a)->subkey - ((tc_item *) b)->subkey;

    return ((tc_item *) a)->key - ((tc_item *) b)->key;
}

static void
transpose(uint32_t *msg, size_t len, char *key)
{
    tc_item *items;
    int i, k;

    if (!(items = malloc(sizeof(tc_item)*len)))
        fatal(out_of_memory);
    for (i = 0, k = 0; i < len; i++) {
        items[i].code = msg[i];
        items[i].key = key[k];
        items[i].subkey = k;
        if (!key[++k])
            k = 0;
    }
    mergesort(items, len, sizeof(tc_item), compare);
    for (i = 0; i < len; i++)
        msg[i] = items[i].code;
    free(items);
}

static void
reverse(uint32_t *msg, size_t len, char *key)
{
    int i, k;

    tc_item *items;
    if (!(items = malloc(sizeof(tc_item)*len)))
        fatal(out_of_memory);
    for (i = 0, k = 0; i < len; i++) {
        items[i].code = i;
        items[i].key = key[k];
        items[i].subkey = k;
        if (!key[++k])
            k = 0;
    }
    mergesort(items, len, sizeof(tc_item), compare);
    for (i = 0, k = 0; i < len; i++) {
        items[i].key = (int) items[i].code;
        items[i].code = msg[i];
    }
    mergesort(items, len, sizeof(tc_item), compare);
    for (i = 0; i < len; i++)
        msg[i] = items[i].code;
    free(items);
}

int
main(int argc, char **argv)
{
    static const struct optparse_name global[] = {
        {"version", 'V', OPTPARSE_NONE},
        {"output",  'o', OPTPARSE_REQUIRED},
        {"simple",  '1', OPTPARSE_NONE},
        {"decrypt", 'd', OPTPARSE_NONE},
        {"encrypt", 'e', OPTPARSE_NONE},
        {"help",    256, OPTPARSE_NONE},
        {0, 0, 0}
    };

    int option, crypt = 1, simple = 0;
    char *infile, *outfile = 0;
    struct optparse options[1];

    FILE *in = stdin, *out = stdout;
    char key1[UDTC_PASSWORD_MAX], key2[UDTC_PASSWORD_MAX];
    uint32_t *udata;
    size_t udata_len;

    optparse_init(options, argv);
    while ((option = optparse(options, global)) != OPTPARSE_DONE) {
        switch (option) {
        case '1':
            simple = 1;
            break;
        case 'd':
            crypt = 0;
            break;
        case 'e':
            crypt = 1;
            break;
        case 'o':
            outfile = options->optarg;
            break;
        case 'V':
            puts("udtc " STR(UDTC_VERSION));
            exit(EXIT_SUCCESS);
        case 256:
            puts(docs_usage);
            printf("\n%s\n", docs_summary);
            exit(EXIT_SUCCESS);
        case OPTPARSE_ERROR:
        default:
            fprintf(stderr, "%s\n", options->errmsg);
            fprintf(stderr, "%s\n", docs_usage);
            exit(EXIT_FAILURE);
        }
    }
    infile = optparse_arg(options);

    if (infile && !(in = fopen(infile, "r")))
        fatal("could not open input file '%s' -- %s",
                infile, strerror(errno));

    if (outfile) {
        if (!(out = fopen(outfile, "w")))
            fatal("could not open output file '%s' -- %s",
                    outfile, strerror(errno));
        cleanup_file = outfile;
    }
    cleanup_fd = out;

    get_key(key1, UDTC_PASSWORD_MAX, "key1: ");
    if (!*key1)
        fatal("key1 has length zero");
    if (!simple) {
        get_key(key2, UDTC_PASSWORD_MAX, "key2: ");
        if (!*key2)
            fatal("key2 has length zero");
    }

    udata = read_all(in, &udata_len);
    if (udata_len > 0) {
        if (crypt) {
            transpose(udata, udata_len, key1);
            if (!simple)
                transpose(udata, udata_len, key2);
        } else {
            if (!simple)
                reverse(udata, udata_len, key2);
            reverse(udata, udata_len, key1);
        }
        write_all(out, udata, udata_len);
    }

    if (in != stdin)
        fclose(in);
    if (out != stdout)
        fclose(out);
    return 0;
}
