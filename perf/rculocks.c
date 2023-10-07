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

int err = 0;
unsigned long *dataval = NULL;
OSSL_TIME reader_end = { 0 };
OSSL_TIME writer_end = { 0 };
int writers = 0;
int readers = 0;
int write_lock_calls = 0;
int read_lock_calls = 0;
int threadcount;
int stop = 0;

CRYPTO_RCU_LOCK *lock = NULL;

void do_rcu_wlock(size_t num)
{
    int i;
    unsigned long *newval, *oldval;
    unsigned long *ldataval;
    int local_write_lock_calls = 0;

    while (stop != 1) {
        newval = OPENSSL_malloc(sizeof(int));        
        CRYPTO_THREAD_rcu_write_lock(lock);
        ldataval = CRYPTO_THREAD_rcu_derefrence(&dataval);
        if (ldataval != NULL)
            *newval = ((*ldataval) + 1);
        else
            *newval = 1;
        CRYPTO_THREAD_rcu_assign_pointer(&dataval, &newval);
        CRYPTO_THREAD_rcu_write_unlock(lock);
        CRYPTO_THREAD_synchronize_rcu(lock);
        OPENSSL_free(ldataval);
        local_write_lock_calls += 2; /* lock and unlock */
    }

    CRYPTO_THREAD_rcu_write_lock(lock);
    write_lock_calls += local_write_lock_calls;
    writers--;
    if (writers == 0)
        writer_end = ossl_time_now();
    CRYPTO_THREAD_rcu_write_unlock(lock);
 
}

void do_rcu_rlock(size_t num)
{
    int i;
    unsigned long last_val = 0;
    unsigned long *ldatavalptr;
    unsigned long ldataval;
    int local_read_lock_calls = 0;

    while (stop != 1) {
        CRYPTO_THREAD_rcu_read_lock(lock);
        ldatavalptr = CRYPTO_THREAD_rcu_derefrence(&dataval);
        ldataval = (ldatavalptr == NULL) ? 0 : *ldatavalptr;
        CRYPTO_THREAD_rcu_read_unlock(lock);
        if (last_val != 0 && last_val > ldataval)
            printf("dataval went backwards! %lu:%lu\n", last_val, ldataval);
        last_val = ldataval;
        local_read_lock_calls += 2; /* lock and unlock */
    }

    CRYPTO_THREAD_rcu_write_lock(lock);
    read_lock_calls += local_read_lock_calls;
    readers--;
    if (readers == 0)
        reader_end = ossl_time_now();
    CRYPTO_THREAD_rcu_write_unlock(lock);
}

void do_rculocks(size_t num)
{
    if (num >= threadcount - writers)
        do_rcu_wlock(num);
    else
        do_rcu_rlock(num);
}

static void handle_alarm(int sig)
{
    stop  = 1;
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
        printf("Usage: rculocks [--terse] threadcount\n");
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

    orig_writers = writers;
    readers = threadcount - writers;

    CRYPTO_THREAD_rcu_init();
    lock = CRYPTO_THREAD_rcu_lock_new();
    if (lock == NULL) {
        printf("unable to allocate lock\n");
        return EXIT_FAILURE;
    }

    signal(SIGALRM, handle_alarm);

    if (!terse)
        printf("Running rculock test with %d writers and %d readers\n", writers, readers);


    alarm(5);

    start = ossl_time_now();

    if (!perflib_run_multi_thread_test(do_rculocks, threadcount, &duration)) {
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
        printf("Average time per CRYPTO_THREAD_rcu_write_lock/unlock call pair: %lfus\n",
               avwcalltime);
        printf("Average time per CRYPTO_THREAD_rcu_read_lock/unlock call pair: %lfus\n",
               avrcalltime);
    }

    CRYPTO_THREAD_rcu_lock_free(lock);
    return EXIT_SUCCESS;
}
