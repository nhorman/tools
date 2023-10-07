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
#include <unistd.h>
#include <signal.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include "perflib/perflib.h"

#define NUM_CALLS_PER_BLOCK         1000
#define NUM_CALL_BLOCKS_PER_THREAD  1000
#define NUM_CALLS_PER_THREAD        (NUM_CALLS_PER_BLOCK * NUM_CALL_BLOCKS_PER_THREAD)

int threadcount = 0;
int err = 0;
unsigned long *dataval = NULL;
int writers = 0;
int readers = 0;
int write_lock_calls = 0;
int read_lock_calls = 0;
OSSL_TIME reader_end = { 0 };
OSSL_TIME writer_end = { 0 };
int stop = 0;

CRYPTO_RWLOCK *lock = NULL;

void do_rw_wlock(size_t num)
{
    int i;
    unsigned long *newval, *oldval;
    int local_write_lock_calls = 0;

    while (stop != 1) {
        newval = OPENSSL_malloc(sizeof(int));        
        CRYPTO_THREAD_write_lock(lock);
        if (dataval == NULL)
            *newval = 1;
        else
            *newval = ((*dataval) + 1);
        oldval = dataval;
        dataval = newval;
        CRYPTO_THREAD_unlock(lock);
        local_write_lock_calls += 2; /* lock and unlock */
        OPENSSL_free(oldval);
    }

    CRYPTO_THREAD_write_lock(lock);
    write_lock_calls += local_write_lock_calls;
    writers--;
    if (writers == 0)
        writer_end = ossl_time_now();
    CRYPTO_THREAD_unlock(lock); 
}

void do_rw_rlock(size_t num)
{
    int i;
    unsigned long last_val = 0;
    int local_read_lock_calls = 0;

    while (stop != 1) {
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
    readers--;
    if (readers == 0)
        reader_end = ossl_time_now();
    CRYPTO_THREAD_unlock(lock);
}

void do_rwlocks(size_t num)
{
    if (num >= threadcount - writers)
        do_rw_wlock(num);
    else
        do_rw_rlock(num);
}

static void handle_alarm(int sig)
{
    stop = 1;
}

int main(int argc, char *argv[])
{
    OSSL_TIME duration;
    OSSL_TIME start;
    uint64_t us;
    double avwcalltime;
    double avrcalltime;
    int terse = 0;
    int argnext;
    char *writeenv;
    int orig_writers;

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
    }

    lock = CRYPTO_THREAD_lock_new();
    if (lock == NULL) {
        printf("unable to allocate lock\n");
        return EXIT_FAILURE;
    }

    orig_writers = writers;
    readers = threadcount - writers;

    if (!terse)
        printf("Running rwlock test with %d writers and %d readers\n", writers, readers);

    signal(SIGALRM, handle_alarm);

    start = ossl_time_now(); 

    alarm(5);

    if (!perflib_run_multi_thread_test(do_rwlocks, threadcount, &duration)) {
        printf("Failed to run the test\n");
        return EXIT_FAILURE;
    }

    if (err) {
        printf("Error during test\n");
        return EXIT_FAILURE;
    }

    if (orig_writers != 0) {
        us = ossl_time2us(ossl_time_subtract(writer_end, start));
        avwcalltime = (double)us / (double)write_lock_calls;
    } else {
        us = 0;
        avwcalltime = 0;
    }

    if (!terse)
        printf("total write lock/unlock calls %d in %lf us\n", write_lock_calls, (double)us);

    us = ossl_time2us(ossl_time_subtract(reader_end, start));
    avrcalltime = (double)us / (double)read_lock_calls;
    if (!terse)
        printf("total read lock/unlock calls %d %lf us\n", read_lock_calls, (double)us);

    if (terse)
        printf("%lf %lf\n", avwcalltime, avrcalltime);
    else {
        printf("Average time per CRYPTO_THREAD_write_lock/unlock call pair: %lfus\n",
               avwcalltime);
        printf("Average time per CRYPTO_THREAD_read_lock/unlock call pair: %lfus\n",
               avrcalltime);
    }

    CRYPTO_THREAD_lock_free(lock);
    return EXIT_SUCCESS;
}
