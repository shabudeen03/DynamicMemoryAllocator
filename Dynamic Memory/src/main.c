#include <stdio.h>
#include "sfmm.h"

#define LOOP_LIMIT 10000

static void test() {
    double actual;
    double expected;
    void * y = sf_malloc(22);
    void * x = sf_malloc(305);

    {
        int *arr[50];
        int i;

        for(i=1; i < 50;i++)
            arr[i] = sf_malloc(10 * i + i);

        for(i=1; i < 50;i++)
            arr[i] = sf_realloc(arr[i], 10 * i);

        for(i=1; i < 50;i++)
            sf_free(arr[i]);
    }

    void * w = sf_malloc(100);
    sf_realloc(w, 40);

    actual = sf_fragmentation();
    expected = 0.819196;
    printf("Actual - %lf, Expected - %lf\n", actual, expected);

    sf_free(w);
    sf_free(x);
    sf_free(y);
}

int main(int argc, char const *argv[]) {
    test();
    return EXIT_SUCCESS;
}
