#include "sfts.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void build_temp_db_path(char * out_path, size_t out_path_size)
{
    const char * tmpl = "/tmp/sfts-tests-XXXXXX";
    size_t tmpl_len = strlen(tmpl);
    assert(out_path_size > tmpl_len);
    memcpy(out_path, tmpl, tmpl_len + 1);
    int fd = mkstemp(out_path);
    assert(fd >= 0);
    close(fd);
    unlink(out_path);
}

static int docs_contains(const uint64_t * docs, size_t count, uint64_t needle)
{
    for (size_t i = 0; i < count; i++) {
        if (docs[i] == needle) {
            return 1;
        }
    }
    return 0;
}

static void test_basic_set_search_remove(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    sfts * index = sfts_new(path);
    assert(index != NULL);
    assert(sfts_open(index) == 0);

    assert(sfts_set(index, 42, "hello world") == 0);

    uint64_t * docs = NULL;
    size_t count = 0;
    assert(sfts_search(index, "hel", sfts_search_kind_prefix, &docs, &count) == 0);
    assert(count == 1);
    assert(docs_contains(docs, count, 42));
    free(docs);

    docs = NULL;
    count = 0;
    assert(sfts_search(index, "orl", sfts_search_kind_substr, &docs, &count) == 0);
    assert(count == 1);
    assert(docs_contains(docs, count, 42));
    free(docs);

    assert(sfts_remove(index, 42) == 0);

    docs = NULL;
    count = 1234;
    assert(sfts_search(index, "hello", sfts_search_kind_prefix, &docs, &count) == 0);
    assert(count == 0);
    free(docs);

    sfts_close(index);
    sfts_free(index);
    unlink(path);
}

static void test_regression_remove_docid_preserves_wordid_encoding(void)
{
    char path[64];
    build_temp_db_path(path, sizeof(path));

    sfts * index = sfts_new(path);
    assert(index != NULL);
    assert(sfts_open(index) == 0);

    // Build three words in this order:
    // wordid(0): alpha -> docs {10, 1}
    // wordid(1): beta  -> docs {3}
    assert(sfts_set(index, 10, "alpha") == 0);
    assert(sfts_set(index, 1, "alpha") == 0);
    assert(sfts_set(index, 3, "beta") == 0);

    // Removing doc 10 from alpha used to corrupt alpha's value encoding to [1]
    // instead of [wordid=0][1].
    assert(sfts_remove(index, 10) == 0);

    // In buggy code this removal decodes wordid=1 from alpha and deletes "/1",
    // which is beta's wordid mapping.
    assert(sfts_remove(index, 1) == 0);

    // If "/1" was wrongly deleted above, removing doc 3 cannot resolve "beta"
    // and stale results remain for "beta".
    assert(sfts_remove(index, 3) == 0);

    uint64_t * docs = NULL;
    size_t count = 0;
    assert(sfts_search(index, "beta", sfts_search_kind_prefix, &docs, &count) == 0);
    assert(count == 0);
    free(docs);

    sfts_close(index);
    sfts_free(index);
    unlink(path);
}

int main(void)
{
    test_basic_set_search_remove();
    test_regression_remove_docid_preserves_wordid_encoding();
    printf("sfts unit tests passed\n");
    return 0;
}
