#include "kvdb.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct enumerate_state {
    int count;
    int stop_after;
};

static void enumerate_count_callback(kvdb * db,
                                     struct kvdb_enumerate_cb_params * params,
                                     void * data,
                                     int * stop)
{
    (void) db;
    (void) params;
    struct enumerate_state * state = (struct enumerate_state *) data;
    state->count++;
    if ((state->stop_after > 0) && (state->count >= state->stop_after)) {
        *stop = 1;
    }
}

static void build_temp_db_path(char * out_path, size_t out_path_size)
{
    const char * tmpl = "/tmp/kvdb-tests-XXXXXX";
    size_t tmpl_len = strlen(tmpl);
    assert(out_path_size > tmpl_len);
    memcpy(out_path, tmpl, tmpl_len + 1);
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    close(fd);
    unlink(out_path);
}

static void test_crud_and_overwrite_raw(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdb * db = kvdb_new(path);
    assert(db != NULL);
    kvdb_set_compression_type(db, KVDB_COMPRESSION_TYPE_RAW);
    assert(kvdb_open(db) == 0);

    char * value = NULL;
    size_t value_size = 0;
    assert(kvdb_get(db, "missing", 7, &value, &value_size) == -1);

    assert(kvdb_set(db, "alpha", 5, "one", 3) == 0);
    assert(kvdb_get(db, "alpha", 5, &value, &value_size) == 0);
    assert(value_size == 3);
    assert(memcmp(value, "one", 3) == 0);
    free(value);

    assert(kvdb_set(db, "alpha", 5, "updated", 7) == 0);
    assert(kvdb_get(db, "alpha", 5, &value, &value_size) == 0);
    assert(value_size == 7);
    assert(memcmp(value, "updated", 7) == 0);
    free(value);

    assert(kvdb_delete(db, "alpha", 5) == 0);
    assert(kvdb_get(db, "alpha", 5, &value, &value_size) == -1);

    kvdb_close(db);
    kvdb_free(db);
    unlink(path);
}

static void test_persistence_lz4(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    const char * key = "persist-key";
    const char * value_in = "This value should survive reopen and decompression.";

    kvdb * db = kvdb_new(path);
    assert(db != NULL);
    assert(kvdb_open(db) == 0);
    assert(kvdb_get_compression_type(db) == KVDB_COMPRESSION_TYPE_LZ4);
    assert(kvdb_set(db, key, strlen(key), value_in, strlen(value_in)) == 0);
    kvdb_close(db);
    kvdb_free(db);

    db = kvdb_new(path);
    assert(db != NULL);
    assert(kvdb_open(db) == 0);
    assert(kvdb_get_compression_type(db) == KVDB_COMPRESSION_TYPE_LZ4);

    char * value_out = NULL;
    size_t value_out_size = 0;
    assert(kvdb_get(db, key, strlen(key), &value_out, &value_out_size) == 0);
    assert(value_out_size == strlen(value_in));
    assert(memcmp(value_out, value_in, value_out_size) == 0);
    free(value_out);

    kvdb_close(db);
    kvdb_free(db);
    unlink(path);
}

static void test_enumerate_and_stop(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdb * db = kvdb_new(path);
    assert(db != NULL);
    assert(kvdb_open(db) == 0);

    const char * long_key =
        "this-key-is-intentionally-long-to-hit-the-slow-read-path-"
        "when-enumerating-over-128-bytes-aaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    assert(strlen(long_key) > 128);

    assert(kvdb_set(db, "k1", 2, "v1", 2) == 0);
    assert(kvdb_set(db, "k2", 2, "v2", 2) == 0);
    assert(kvdb_set(db, long_key, strlen(long_key), "v3", 2) == 0);

    struct enumerate_state state;
    state.count = 0;
    state.stop_after = 0;
    assert(kvdb_enumerate_keys(db, enumerate_count_callback, &state) == 0);
    assert(state.count == 3);

    state.count = 0;
    state.stop_after = 2;
    assert(kvdb_enumerate_keys(db, enumerate_count_callback, &state) == 0);
    assert(state.count == 2);

    kvdb_close(db);
    kvdb_free(db);
    unlink(path);
}

static void test_compression_change_ignored_while_open(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdb * db = kvdb_new(path);
    assert(db != NULL);
    kvdb_set_compression_type(db, KVDB_COMPRESSION_TYPE_RAW);
    assert(kvdb_open(db) == 0);
    assert(kvdb_get_compression_type(db) == KVDB_COMPRESSION_TYPE_RAW);

    kvdb_set_compression_type(db, KVDB_COMPRESSION_TYPE_LZ4);
    assert(kvdb_get_compression_type(db) == KVDB_COMPRESSION_TYPE_RAW);

    kvdb_close(db);
    kvdb_free(db);
    unlink(path);
}

static void test_zero_size_value_lz4(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdb * db = kvdb_new(path);
    assert(db != NULL);
    assert(kvdb_open(db) == 0);
    assert(kvdb_set(db, "zero", 4, "", 0) == 0);

    char * value = NULL;
    size_t value_size = 1234;
    assert(kvdb_get(db, "zero", 4, &value, &value_size) == 0);
    assert(value_size == 0);
    assert(value == NULL);

    kvdb_close(db);
    kvdb_free(db);
    unlink(path);
}

int main(void)
{
    test_crud_and_overwrite_raw();
    test_persistence_lz4();
    test_enumerate_and_stop();
    test_compression_change_ignored_while_open();
    test_zero_size_value_lz4();
    printf("kvdb unit tests passed\n");
    return 0;
}
