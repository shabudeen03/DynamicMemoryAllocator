#define TEST_TIMEOUT 15
#include "__grading_helpers.h"
#include "debug.h"



/*
 * Check LIFO discipline on free list
 */
Test(sf_memsuite_grading, malloc_free_lifo, .timeout=TEST_TIMEOUT)
{
    size_t sz = 200;
    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 16), 1, 224);
    void * u = sf_malloc(sz);
    _assert_nonnull_payload_pointer(u);
    _assert_block_info((sf_block *)((char *)u - 16), 1, 224);
    void * y = sf_malloc(sz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 16), 1, 224);
    void * v = sf_malloc(sz);
    _assert_nonnull_payload_pointer(v);
    _assert_block_info((sf_block *)((char *)v - 16), 1, 224);
    void * z = sf_malloc(sz);
    _assert_nonnull_payload_pointer(z);
    _assert_block_info((sf_block *)((char *)z - 16), 1, 224);
    void * w = sf_malloc(sz);
    _assert_nonnull_payload_pointer(w);
    _assert_block_info((sf_block *)((char *)w - 16), 1, 224);

    sf_free(x);
    sf_free(y);
    sf_free(z);

    void * z1 = sf_malloc(sz);
    _assert_nonnull_payload_pointer(z1);
    _assert_block_info((sf_block *)((char *)z1 - 16), 1, 224);
    void * y1 = sf_malloc(sz);
    _assert_nonnull_payload_pointer(y1);
    _assert_block_info((sf_block *)((char *)y1 - 16), 1, 224);
    void * x1 = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x1);
    _assert_block_info((sf_block *)((char *)x1 - 16), 1, 224);

    cr_assert(x == x1 && y == y1 && z == z1,
      "malloc/free does not follow LIFO discipline");

    _assert_free_block_count(2704, 1);

    _assert_errno_eq(0);
}

/*
 * Realloc tests.
 */
Test(sf_memsuite_grading, realloc_larger, .timeout=TEST_TIMEOUT)
{
    size_t sz = 200;
    size_t nsz = 1024;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 16), 1, 224);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 16), 1, 1040);

    _assert_free_block_count(224, 1);

    _assert_free_block_count(2784, 1);

    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_smaller, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    size_t nsz = 200;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 16), 1, 1040);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 16), 1, 224);

    cr_assert_eq(x, y, "realloc to smaller size did not return same payload pointer");

    _assert_free_block_count(3824, 1);
    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_same, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    size_t nsz = 1024;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 16), 1, 1040);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y- 16), 1, 1040);

    cr_assert_eq(x, y, "realloc to same size did not return same payload pointer");

    _assert_free_block_count(3008, 1);

    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_splinter, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    size_t nsz = 1020;

    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 16), 1, 1040);

    void * y = sf_realloc(x, nsz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 16), 1, 1040);

    cr_assert_eq(x, y, "realloc to smaller size did not return same payload pointer");

    _assert_free_block_count(3008, 1);
    _assert_errno_eq(0);
}

Test(sf_memsuite_grading, realloc_size_0, .timeout=TEST_TIMEOUT)
{
    size_t sz = 1024;
    void * x = sf_malloc(sz);
    _assert_nonnull_payload_pointer(x);
    _assert_block_info((sf_block *)((char *)x - 16), 1, 1040);

    void * y = sf_malloc(sz);
    _assert_nonnull_payload_pointer(y);
    _assert_block_info((sf_block *)((char *)y - 16), 1, 1040);

    void * z = sf_realloc(x, 0);
    _assert_null_payload_pointer(z);
    _assert_block_info((sf_block *)((char *)x - 16), 0, 1040);

    // after realloc x to (2) z, x is now a free block
    _assert_free_block_count(1040, 1);

    // the size of the remaining free block
    _assert_free_block_count(1968, 1);

    _assert_errno_eq(0);
}

/*
 * Illegal pointer tests.
 */
Test(sf_memsuite_grading, free_null, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    (void) sf_malloc(sz);
    sf_free(NULL);
    cr_assert_fail("SIGABRT should have been received");
}

//This test tests: Freeing a memory that was free-ed already
Test(sf_memsuite_grading, free_unallocated, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    void *x = sf_malloc(sz);
    sf_free(x);
    sf_free(x);
    cr_assert_fail("SIGABRT should have been received");
}

Test(sf_memsuite_grading, free_block_too_small, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    void * x = sf_malloc(sz);

    ((sf_block *)((char *)x - 16))->header = 0x0UL;
    //PAYLOAD_TO_BLOCK(x)->header ^= MAGIC;

    sf_free(x);
    cr_assert_fail("SIGABRT should have been received");
}

Test(sf_memsuite_grading, free_prev_alloc, .signal = SIGABRT, .timeout = TEST_TIMEOUT)
{
    size_t sz = 1;
    void * w = sf_malloc(sz);
    void * x = sf_malloc(sz);
    ((sf_block *)((char *)x - 16))->header &= ~0x4;
    sf_free(x);
    sf_free(w);
    cr_assert_fail("SIGABRT should have been received");
}

// random block assigments. Tried to give equal opportunity for each possible order to appear.
// But if the heap gets populated too quickly, try to make some space by realloc(half) existing
// allocated blocks.
Test(sf_memsuite_grading, stress_test, .timeout = TEST_TIMEOUT)
{
    errno = 0;

    int order_range = 13;
    int nullcount = 0;

    void * tracked[100];

    for (int i = 0; i < 100; i++)
    {
        int order = (rand() % order_range);
        size_t extra = (rand() % (1 << order));
        size_t req_sz = (1 << order) + extra;

        tracked[i] = sf_malloc(req_sz);
        // if there is no free to malloc
        if (tracked[i] == NULL)
        {
            order--;
            while (order >= 0)
            {
                req_sz = (1 << order) + (extra % (1 << order));
                tracked[i] = sf_malloc(req_sz);
                if (tracked[i] != NULL)
                {
                    break;
                }
                else
                {
                    order--;
                }
            }
        }

        // tracked[i] can still be NULL
        if (tracked[i] == NULL)
        {
            nullcount++;
            // It seems like there is not enough space in the heap.
            // Try to halve the size of each existing allocated block in the heap,
            // so that next mallocs possibly get free blocks.
            for (int j = 0; j < i; j++)
            {
                if (tracked[j] == NULL)
                {
                    continue;
                }
                sf_block * bp = (sf_block *)((char *)tracked[j] - 16);
                req_sz = (bp->header & ~0xfU) >> 1;
                tracked[j] = sf_realloc(tracked[j], req_sz);
            }
        }
        errno = 0;
    }

    for (int i = 0; i < 100; i++)
    {
        if (tracked[i] != NULL)
        {
            sf_free(tracked[i]);
        }
    }

    _assert_heap_is_valid();
}

// Statistics tests.

static size_t max_aggregate_payload = 0;
static size_t current_aggregate_payload = 0;
static size_t total_allocated_block_size = 0;

static void tally_alloc(void *p, size_t size) {
    sf_block *bp = (sf_block *)((char *)p - 16);
    if(bp) {
	current_aggregate_payload += size;
	total_allocated_block_size += bp->header & ~0xfU;
	if(current_aggregate_payload > total_allocated_block_size) {
	    fprintf(stderr,
		    "Aggregate payload (%lu) > total allocated block size (%lu) (not possible!)\n",
		    current_aggregate_payload, total_allocated_block_size);
	    abort();
	}
	if(current_aggregate_payload > max_aggregate_payload)
	    max_aggregate_payload = current_aggregate_payload;
    }
}

static void tally_free(void *p) {
    sf_block *bp = (sf_block *)((char *)p - 16);
    current_aggregate_payload -= (bp->header >> 32) & 2147483648;
    total_allocated_block_size -= bp->header & ~0xf;
    if(current_aggregate_payload > total_allocated_block_size) {
	fprintf(stderr,
		"Aggregate payload (%lu) > total allocated block size (%lu) (not possible!)\n",
		current_aggregate_payload, total_allocated_block_size);
	abort();
    }
}

static void *SF_MALLOC(size_t size) {
    void *p = sf_malloc(size);
    tally_alloc(p, size);
    return p;
}

static void *SF_REALLOC(void *ptr, size_t size) {
    tally_free(ptr);
    void *p = sf_realloc(ptr, size);
    tally_alloc(p, size);
    return p;
}

static void SF_FREE(void *ptr) {
    tally_free(ptr);
    sf_free(ptr);
}

static double ref_sf_fragmentation() {
    double ret;
    if (sf_mem_end() == sf_mem_start())
        return 0;
    ret = (double)current_aggregate_payload / total_allocated_block_size;
    debug("internal fragmentation: %f", ret);
    return ret;
}

static double ref_sf_utilization() {
    double ret;
    if (sf_mem_end() == sf_mem_start())
        return 0;
    ret = (double)max_aggregate_payload / (sf_mem_end() - sf_mem_start());
    debug("peak utilization: %f", ret);
    return ret;
}

Test(sf_memsuite_stats, peak_utilization, .timeout = TEST_TIMEOUT)
{
    double actual;
    double expected;
    void * w = SF_MALLOC(10);
    void * x = SF_MALLOC(300);
    void * y = SF_MALLOC(500);
    SF_FREE(x);

    actual = sf_utilization();
    expected = ref_sf_utilization();
    expected = 0.197754;

    cr_assert_float_eq(actual, expected, 0.0001, "peak utilization_1 did not match (exp=%f, found=%f)", expected, actual);

    x = SF_MALLOC(400);
    void * z = SF_MALLOC(1024);
    SF_FREE(w);
    SF_FREE(x);
    x = SF_MALLOC(2048);

    actual = sf_utilization();
    expected = ref_sf_utilization();
    expected = 0.436035;

    cr_assert_float_eq(actual, expected, 0.0001, "peak utilization_2 did not match (exp=%f, found=%f)", expected, actual);

    SF_FREE(x);
    x = SF_MALLOC(7400);
    SF_FREE(y);
    SF_FREE(z);

    actual = sf_utilization();
    expected = ref_sf_utilization();
    expected = 0.726237;

    cr_assert_float_eq(actual, expected, 0.0001, "peak utilization_3 did not match (exp=%f, found=%f)", expected, actual);
}

Test(sf_memsuite_stats, internal_fragmentation, .timeout = TEST_TIMEOUT)
{
    double actual;
    double expected;
    void * w = SF_MALLOC(22);
    void * x = SF_MALLOC(305);
    actual = sf_fragmentation();
    expected = ref_sf_fragmentation();
    expected = 0.851562;

    cr_assert_float_eq(actual, expected, 0.0001, "internal fragmentation_1 did not match (exp=%f, found=%f)", expected, actual);

    void * y = SF_MALLOC(1500);
    SF_FREE(x);
    x = SF_MALLOC(2048);

    void * z = SF_MALLOC(526);
    actual = sf_fragmentation();
    expected = ref_sf_fragmentation();
    expected = 0.980843;

    cr_assert_float_eq(actual, expected, 0.0001, "internal fragmentation_2 did not match (exp=%f, found=%f)", expected, actual);

    SF_FREE(z);
    z = SF_MALLOC(700);
    SF_FREE(x);
    x = SF_MALLOC(4096);
    SF_FREE(w);
    SF_FREE(x);

    {
        int *arr[50];
        int i;

        for(i=1; i < 50;i++)
            arr[i] = SF_MALLOC(10 * i + i);

        actual = sf_fragmentation();
	expected = ref_sf_fragmentation();
	expected = 0.929495;

	cr_assert_float_eq(actual, expected, 0.0001, "internal fragmentation_3 did not match (exp=%f, found=%f)", expected, actual);

        for(i=1; i < 50;i++)
            SF_FREE(arr[i]);
    }

    SF_FREE(y);
    SF_FREE(z);
}

Test(sf_memsuite_stats, realloc_internal_fragmentation, .timeout = TEST_TIMEOUT)
{
    double actual;
    double expected;
    void * y = SF_MALLOC(22);
    void * x = SF_MALLOC(305);

    {
        int *arr[50];
        int i;

        for(i=1; i < 50;i++)
            arr[i] = SF_MALLOC(10 * i + i);

        for(i=1; i < 50;i++)
            arr[i] = SF_REALLOC(arr[i], 10 * i);

        for(i=1; i < 50;i++)
            SF_FREE(arr[i]);
    }

    void * w = SF_MALLOC(100);
    SF_REALLOC(w, 40);

    actual = sf_fragmentation();
    expected = ref_sf_fragmentation();
    expected = 0.819196;

    cr_assert_float_eq(actual, expected, 0.0001, "internal fragmentation did not match (exp=%f, found=%f)", expected, actual);

    SF_FREE(w);
    SF_FREE(x);
    SF_FREE(y);
}
