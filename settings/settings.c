#include <kvdb/kvdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DB_PATH "settings.kvdb"

struct list_ctx {
    kvdb *db;
    int rc;
};

static const char *db_path_from_env_or_default(void)
{
    const char *p = getenv("SETTINGS_DB");
    if (p != NULL && p[0] != '\0') {
        return p;
    }
    return DEFAULT_DB_PATH;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-f <db-file>] <command> [args...]\n"
            "\n"
            "Commands:\n"
            "  list                  List all settings as key=value pairs\n"
            "  get <key>             Print the value bound to <key>\n"
            "  put <key> <value>     Store <value> under <key>\n"
            "  delete <key>          Remove <key> from the database\n"
            "\n"
            "Options:\n"
            "  -f <db-file>          Path to the kvdb file (default: %s,\n"
            "                        overridable via $SETTINGS_DB)\n",
            prog, DEFAULT_DB_PATH);
}

static void print_escaped(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        unsigned char c = (unsigned char) data[i];
        if (c == '\\') {
            fputs("\\\\", stdout);
        } else if (c == '\n') {
            fputs("\\n", stdout);
        } else if (c == '\r') {
            fputs("\\r", stdout);
        } else if (c == '\t') {
            fputs("\\t", stdout);
        } else if (c < 0x20 || c == 0x7f) {
            printf("\\x%02x", c);
        } else {
            fputc((int) c, stdout);
        }
    }
}

static void list_callback(kvdb *db,
                          struct kvdb_enumerate_cb_params *params,
                          void *data,
                          int *stop)
{
    (void) stop;
    struct list_ctx *ctx = (struct list_ctx *) data;
    if (ctx->rc != 0) {
        *stop = 1;
        return;
    }

    char *value = NULL;
    size_t value_size = 0;
    int r = kvdb_get(db, params->key, params->key_size, &value, &value_size);
    if (r == -2) {
        fprintf(stderr, "I/O error reading value during list\n");
        ctx->rc = -2;
        *stop = 1;
        return;
    }

    print_escaped(params->key, params->key_size);
    fputc('=', stdout);
    if (r == 0) {
        print_escaped(value, value_size);
        free(value);
    }
    fputc('\n', stdout);
}

static int cmd_list(kvdb *db)
{
    struct list_ctx ctx;
    ctx.db = db;
    ctx.rc = 0;

    int r = kvdb_enumerate_keys(db, list_callback, &ctx);
    if (r == -2) {
        fprintf(stderr, "I/O error while enumerating keys\n");
        return 1;
    }
    if (ctx.rc != 0) {
        return 1;
    }
    return 0;
}

static int cmd_get(kvdb *db, const char *key)
{
    char *value = NULL;
    size_t value_size = 0;
    int r = kvdb_get(db, key, strlen(key), &value, &value_size);
    if (r == -1) {
        fprintf(stderr, "key not found: %s\n", key);
        return 2;
    }
    if (r == -2) {
        fprintf(stderr, "I/O error reading key: %s\n", key);
        return 1;
    }
    fwrite(value, 1, value_size, stdout);
    fputc('\n', stdout);
    free(value);
    return 0;
}

static int cmd_put(kvdb *db, const char *key, const char *value)
{
    int r = kvdb_set(db, key, strlen(key), value, strlen(value));
    if (r == -2) {
        fprintf(stderr, "I/O error writing key: %s\n", key);
        return 1;
    }
    if (r != 0) {
        fprintf(stderr, "failed to store key: %s (code %d)\n", key, r);
        return 1;
    }
    return 0;
}

static int cmd_delete(kvdb *db, const char *key)
{
    int r = kvdb_delete(db, key, strlen(key));
    if (r == -1) {
        fprintf(stderr, "key not found: %s\n", key);
        return 2;
    }
    if (r == -2) {
        fprintf(stderr, "I/O error deleting key: %s\n", key);
        return 1;
    }
    if (r != 0) {
        fprintf(stderr, "failed to delete key: %s (code %d)\n", key, r);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *prog = (argc > 0) ? argv[0] : "settings";
    const char *db_path = db_path_from_env_or_default();

    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(prog);
            return 0;
        }
        if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "-f requires a path argument\n");
                usage(prog);
                return 64;
            }
            db_path = argv[i + 1];
            i += 2;
            continue;
        }
        fprintf(stderr, "unknown option: %s\n", argv[i]);
        usage(prog);
        return 64;
    }

    if (i >= argc) {
        usage(prog);
        return 64;
    }

    const char *cmd = argv[i++];

    kvdb *db = kvdb_new(db_path);
    if (db == NULL) {
        fprintf(stderr, "failed to allocate kvdb handle\n");
        return 1;
    }
    if (kvdb_open(db) < 0) {
        fprintf(stderr, "failed to open database: %s\n", db_path);
        kvdb_free(db);
        return 1;
    }

    int rc = 0;
    if (strcmp(cmd, "list") == 0) {
        if (i != argc) {
            fprintf(stderr, "list takes no arguments\n");
            rc = 64;
        } else {
            rc = cmd_list(db);
        }
    } else if (strcmp(cmd, "get") == 0) {
        if (i + 1 != argc) {
            fprintf(stderr, "get requires exactly one <key> argument\n");
            rc = 64;
        } else {
            rc = cmd_get(db, argv[i]);
        }
    } else if (strcmp(cmd, "put") == 0) {
        if (i + 2 != argc) {
            fprintf(stderr, "put requires <key> and <value> arguments\n");
            rc = 64;
        } else {
            rc = cmd_put(db, argv[i], argv[i + 1]);
        }
    } else if (strcmp(cmd, "delete") == 0) {
        if (i + 1 != argc) {
            fprintf(stderr, "delete requires exactly one <key> argument\n");
            rc = 64;
        } else {
            rc = cmd_delete(db, argv[i]);
        }
    } else {
        fprintf(stderr, "unknown command: %s\n", cmd);
        usage(prog);
        rc = 64;
    }

    kvdb_close(db);
    kvdb_free(db);
    return rc;
}
