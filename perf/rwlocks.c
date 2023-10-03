/*
 * Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include "perflib/perflib.h"

#define NUM_CALLS_PER_BLOCK         100
#define NUM_CALL_BLOCKS_PER_THREAD  100
#define NUM_CALLS_PER_THREAD        (NUM_CALLS_PER_BLOCK * NUM_CALL_BLOCKS_PER_THREAD)

int err = 0;
unsigned long *dataval = NULL;
int writers = 0;
int write_lock_calls = 0;
int read_lock_calls = 0;

CRYPTO_RWLOCK *lock = NULL;

void do_rw_wlock(size_t num)
{
    int i;
    unsigned long *newval, *oldval;

    for (i = 0; i < NUM_CALLS_PER_THREAD; i++) {
        newval = OPENSSL_malloc(sizeof(int));        
        CRYPTO_THREAD_write_lock(lock);
        if (dataval == NULL)
            *newval = 1;
        else
            *newval = ((*dataval) + 1);
        oldval = dataval;
        dataval = newval;
        CRYPTO_THREAD_unlock(lock);
        write_lock_calls += 2; /* lock and unlock */
        OPENSSL_free(oldval);
    }
 
}

void do_rw_rlock(size_t num)
{
    int i;
    unsigned long last_val = 0;
    int local_read_lock_calls = 0;

    for (i = 0; i < NUM_CALLS_PER_THREAD; i++) {
        CRYPTO_THREAD_read_lock(lock);
        if (dataval != NULL) {
            if (last_val != 0 && last_val > *dataval)
                printf("dataval went backwards! %lu:%lu\n", last_val, *dataval);
            last_val = *dataval;
        }
        CRYPTO_THREAD_unlock(lock);
        local_read_lock_calls += 2; /* lock and unlock */
    }

    CRYPTO_THREAD_write_lock(lock);
    read_lock_calls += local_read_lock_calls;
    CRYPTO_THREAD_unlock(lock);
}

void do_rwlocks(size_t num)
{
    if (num < writers)
        do_rw_wlock(num);
    else
        do_rw_rlock(num);
}

int main(int argc, char *argv[])
{
    int threadcount;
    OSSL_TIME duration;
    uint64_t us;
    double avwcalltime;
    double avrcalltime;
    int terse = 0;
    int argnext;
    char *writeenv;

    if ((argc != 2 && argc != 3)
                || (argc == 3 && strcmp("--terse", argv[1]) != 0)) {
        printf("Usage: rwlocks [--terse] threadcount\n");
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        terse = 1;
        argnext = 2;
    } else {
        argnext = 1;
    }

    threadcount = atoi(argv[argnext]);
    if (threadcount < 1) {
        printf("threadcount must be > 0\n");
        return EXIT_FAILURE;
    }

    writeenv=getenv("LOCK_WRITERS");
    if (writeenv == NULL) {
        writers = threadcount / 2;
    } else {
        writers=atoi(writeenv);
        if (writers == 0)
            writers = threadcount / 2;
    }

    lock = CRYPTO_THREAD_lock_new();
    if (lock == NULL) {
        printf("unable to allocate lock\n");
        return EXIT_FAILURE;
    }

    printf("Running rwlock test with %d writers and %d readers\n", writers, threadcount - writers);

    if (!perflib_run_multi_thread_test(do_rwlocks, threadcount, &duration)) {
        printf("Failed to run the test\n");
        return EXIT_FAILURE;
    }

    if (err) {
        printf("Error during test\n");
        return EXIT_FAILURE;
    }

    us = ossl_time2us(duration);

    avwcalltime = (double)us / (double)write_lock_calls;
    avrcalltime = (double)us / (double)read_lock_calls;

    printf("total write lock/unlock calls %d\n", write_lock_calls);
    printf("total read lock/unlock calls %d\n", read_lock_calls);
    if (terse)
        printf("%lf %lf\n", avwcalltime, avrcalltime);
    else {
        printf("Average time per %d CRYPTO_THREAD_write_lock/unlock calls: %lfus\n",
               NUM_CALLS_PER_BLOCK, avwcalltime);
        printf("Average time per %d CRYPTO_THREAD_read_lock/unlock calls: %lfus\n",
               NUM_CALLS_PER_BLOCK, avrcalltime);
    }

    CRYPTO_THREAD_lock_free(lock);
    return EXIT_SUCCESS;
}
