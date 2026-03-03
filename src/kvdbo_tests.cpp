#include "kvdbo.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

static void build_temp_db_path(char * out_path, size_t out_path_size)
{
    const char * tmpl = "/tmp/kvdbo-tests-XXXXXX";
    size_t tmpl_len = strlen(tmpl);
    assert(out_path_size > tmpl_len);
    memcpy(out_path, tmpl, tmpl_len + 1);
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    close(fd);
    unlink(out_path);
}

static std::vector<std::string> iterator_collect_keys(kvdbo * db)
{
    std::vector<std::string> keys;
    kvdbo_iterator * it = kvdbo_iterator_new(db);
    assert(it != NULL);

    kvdbo_iterator_seek_first(it);
    while (kvdbo_iterator_is_valid(it)) {
        const char * key = NULL;
        size_t key_size = 0;
        kvdbo_iterator_get_key(it, &key, &key_size);
        assert(key != NULL);
        keys.push_back(std::string(key, key_size));
        kvdbo_iterator_next(it);
    }

    kvdbo_iterator_free(it);
    return keys;
}

static void test_crud_persistence_and_iterator_order(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdbo * db = kvdbo_new(path);
    assert(db != NULL);
    assert(kvdbo_open(db) == 0);

    char * value = NULL;
    size_t value_size = 0;
    assert(kvdbo_get(db, "missing", 7, &value, &value_size) == -1);

    assert(kvdbo_set(db, "beta", 4, "2", 1) == 0);
    assert(kvdbo_set(db, "alpha", 5, "1", 1) == 0);
    assert(kvdbo_set(db, "gamma", 5, "3", 1) == 0);
    assert(kvdbo_set(db, "beta", 4, "22", 2) == 0);
    assert(kvdbo_flush(db) == 0);

    assert(kvdbo_get(db, "beta", 4, &value, &value_size) == 0);
    assert(value_size == 2);
    assert(memcmp(value, "22", 2) == 0);
    free(value);

    std::vector<std::string> keys = iterator_collect_keys(db);
    assert(keys.size() == 3);
    assert(keys[0] == "alpha");
    assert(keys[1] == "beta");
    assert(keys[2] == "gamma");

    kvdbo_close(db);
    kvdbo_free(db);

    db = kvdbo_new(path);
    assert(db != NULL);
    assert(kvdbo_open(db) == 0);
    assert(kvdbo_get(db, "beta", 4, &value, &value_size) == 0);
    assert(value_size == 2);
    assert(memcmp(value, "22", 2) == 0);
    free(value);
    kvdbo_close(db);
    kvdbo_free(db);

    unlink(path);
}

static void test_invalid_metakey_prefix_rejected(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdbo * db = kvdbo_new(path);
    assert(db != NULL);
    assert(kvdbo_open(db) == 0);

    const char invalid_key[] = {0, 'k', 'v', 'd', 'b', 'o', 'x'};
    assert(kvdbo_set(db, invalid_key, sizeof(invalid_key), "v", 1) == -3);

    char * value = NULL;
    size_t value_size = 0;
    assert(kvdbo_get(db, invalid_key, sizeof(invalid_key), &value, &value_size) == -1);

    kvdbo_close(db);
    kvdbo_free(db);
    unlink(path);
}

static void test_iterator_seek_and_navigation(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdbo * db = kvdbo_new(path);
    assert(db != NULL);
    assert(kvdbo_open(db) == 0);

    assert(kvdbo_set(db, "alpha", 5, "1", 1) == 0);
    assert(kvdbo_set(db, "beta", 4, "2", 1) == 0);
    assert(kvdbo_set(db, "delta", 5, "3", 1) == 0);
    assert(kvdbo_set(db, "omega", 5, "4", 1) == 0);
    assert(kvdbo_flush(db) == 0);

    kvdbo_iterator * it = kvdbo_iterator_new(db);
    assert(it != NULL);

    const char * key = NULL;
    size_t key_size = 0;

    kvdbo_iterator_seek_after(it, "beta", 4);
    assert(kvdbo_iterator_is_valid(it));
    kvdbo_iterator_get_key(it, &key, &key_size);
    assert(std::string(key, key_size) == "beta");

    kvdbo_iterator_seek_after(it, "betz", 4);
    assert(kvdbo_iterator_is_valid(it));
    kvdbo_iterator_get_key(it, &key, &key_size);
    assert(std::string(key, key_size) == "delta");

    kvdbo_iterator_seek_after(it, "z", 1);
    assert(!kvdbo_iterator_is_valid(it));

    kvdbo_iterator_seek_last(it);
    assert(kvdbo_iterator_is_valid(it));
    kvdbo_iterator_get_key(it, &key, &key_size);
    assert(std::string(key, key_size) == "omega");

    kvdbo_iterator_previous(it);
    assert(kvdbo_iterator_is_valid(it));
    kvdbo_iterator_get_key(it, &key, &key_size);
    assert(std::string(key, key_size) == "delta");

    kvdbo_iterator_free(it);
    kvdbo_close(db);
    kvdbo_free(db);
    unlink(path);
}

static void test_delete_visibility_requires_flush_for_iteration(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    kvdbo * db = kvdbo_new(path);
    assert(db != NULL);
    assert(kvdbo_open(db) == 0);

    assert(kvdbo_set(db, "alive", 5, "1", 1) == 0);
    assert(kvdbo_set(db, "gone", 4, "1", 1) == 0);
    assert(kvdbo_flush(db) == 0);

    assert(kvdbo_delete(db, "gone", 4) == 0);

    char * value = NULL;
    size_t value_size = 0;
    assert(kvdbo_get(db, "gone", 4, &value, &value_size) == -1);

    std::vector<std::string> keys_before_flush = iterator_collect_keys(db);
    assert(keys_before_flush.size() == 2);
    assert(keys_before_flush[0] == "alive");
    assert(keys_before_flush[1] == "gone");

    assert(kvdbo_flush(db) == 0);

    std::vector<std::string> keys_after_flush = iterator_collect_keys(db);
    assert(keys_after_flush.size() == 1);
    assert(keys_after_flush[0] == "alive");

    kvdbo_close(db);
    kvdbo_free(db);
    unlink(path);
}

int main(void)
{
    test_crud_persistence_and_iterator_order();
    test_invalid_metakey_prefix_rejected();
    test_iterator_seek_and_navigation();
    test_delete_visibility_requires_flush_for_iteration();
    printf("kvdbo unit tests passed\n");
    return 0;
}
