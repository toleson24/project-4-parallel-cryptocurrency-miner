/**
 * @file mine.c
 *
 * Parallelizes the hash inversion technique used by cryptocurrencies such as
 * bitcoin.
 *
 * Input:    Number of threads, block difficulty, and block contents (string)
 * Output:   Hash inversion solution (nonce) and timing statistics.
 *
 * Compile:  (run make)
 *           When your code is ready for performance testing, you can add the
 *           -O3 flag to enable all compiler optimizations.
 *
 * Run:      ./miner 4 24 'Hello CS 521!!!'
 *
 *   Number of threads: 4
 *     Difficulty Mask: 00000000000000000000000011111111
 *          Block data: [Hello CS 521!!!]
 *
 *   ----------- Starting up miner threads!  -----------
 *
 *   Solution found by thread 3:
 *   Nonce: 10211906
 *   Hash: 0000001209850F7AB3EC055248EE4F1B032D39D0
 *   10221196 hashes in 0.26s (39312292.30 hashes/sec)
 */

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "sha1.h"

unsigned long long total_inversions;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condp = PTHREAD_COND_INITIALIZER;
pthread_cond_t condc = PTHREAD_COND_INITIALIZER;
double condition = 0;
uint64_t global_nonce = 0;
char *global_bitcoin_block_data;
uint32_t global_difficulty_mask;
uint8_t global_digest;

double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void print_binary32(uint32_t num) {
    int i;
    for (i = 31; i >= 0; --i) {
        uint32_t position = (unsigned int) 1 << i;
        printf("%c", ((num & position) == position) ? '1' : '0');
    }
    puts("");
}

uint64_t mine(char *data_block, uint32_t difficulty_mask,
        uint64_t nonce_start, uint64_t nonce_end,
        uint8_t digest[SHA1_HASH_SIZE]) {

    for (uint64_t nonce = nonce_start; nonce < nonce_end; nonce++) {
        /* A 64-bit unsigned number can be up to 20 characters  when printed: */
        size_t buf_sz = sizeof(char) * (strlen(data_block) + 20 + 1);
        char *buf = malloc(buf_sz);

        /* Create a new string by concatenating the block and nonce string.
         * For example, if we have 'Hello World!' and '10', the new string
         * is: 'Hello World!10' */
        snprintf(buf, buf_sz, "%s%lu", data_block, nonce);

        /* Hash the combined string */
        sha1sum(digest, (uint8_t *) buf, strlen(buf));
        free(buf);
        total_inversions++;

        /* Get the first 32 bits of the hash */
        uint32_t hash_front = 0;
        hash_front |= digest[0] << 24;
        hash_front |= digest[1] << 16;
        hash_front |= digest[2] << 8;
        hash_front |= digest[3];

        /* Check to see if we've found a solution to our block */
        if ((hash_front & difficulty_mask) == hash_front) {
            return nonce;
        }
    }

    return 0;
}

void *producer_thread(void *ptr) {
    int i = 1;
    while (true) {
        // add to buffer (i.e. shared memory)
        pthread_mutex_lock(&mutex);
        printf("producer\n");
        if (global_nonce != 0) {
            //break;
            pthread_exit(0);
        }
        while (condition != 0) {
            pthread_cond_wait(&condp, &mutex);
        }
        // produce
        i += 100;
        condition = i;
        pthread_cond_signal(&condc);
        pthread_mutex_unlock(&mutex);  
    }
    return 0;
}

void *consumer_thread(void *ptr) { 
    while (true) {
        int local;
        // remove from buffer (i.e. shared memory)
        pthread_mutex_lock(&mutex);
        printf("consumer\n");
        while (condition = 0) {//} && ) {
            pthread_cond_wait(&condc, &mutex);
        }
        local = condition;
        // consume
        //uint64_t nonce = mine(
        if (global_nonce = mine(
                global_bitcoin_block_data,
                global_difficulty_mask,
                local, local + 100,
                &global_digest
                ) != 0) {
                    //return global_nonce; // is this the right logic??
                    //break;
                    pthread_exit(0);
        }
        condition = 0;
        pthread_cond_signal(&condp);
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Usage: %s threads difficulty 'block data (string)'\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[1]);
    if (num_threads < 1) {
        num_threads = 1;
    }
    printf("Number of threads: %d\n", num_threads);

    int difficulty = atoi(argv[2]);
    printf("Number of leading 0s: %d\n", difficulty);

    //uint32_t difficulty_mask = 0x00000FFF; // 4095
    //difficulty_mask = 4095; // exactly the same as above
    uint32_t difficulty_mask = 0x0;
    for (int i = 0; i < 32 - difficulty; ++i) {
        difficulty_mask = difficulty_mask | 1 << i;
    }
    global_difficulty_mask = difficulty_mask;
    printf("  Difficulty Mask: ");
    print_binary32(difficulty_mask);

    /* We use the input string passed in (argv[3]) as our block data. In a
     * complete bitcoin miner implementation, the block data would be composed
     * of bitcoin transactions. */
    char *bitcoin_block_data = argv[3];
    strncpy(global_bitcoin_block_data, bitcoin_block_data, strlen(bitcoin_block_data)); // more efficient to skip this step entirely : make bitcoin_block_data a global char * ?
    printf("       Block data: [%s]\n", bitcoin_block_data);

    printf("\n----------- Starting up miner threads!  -----------\n\n");

    double start_time = get_time();

    uint8_t digest[SHA1_HASH_SIZE];
    global_digest = *digest;

    /* Mine the block. */
    /*uint64_t nonce = mine(
            bitcoin_block_data,
            difficulty_mask,
            1, UINT64_MAX,
            digest);
    */

    /*----------------------------------------------------------------*/
    pthread_t prod;                                                     // = NULL; // ??
    pthread_create(&prod, NULL, producer_thread, NULL);
    pthread_t consumers[num_threads];                                   // = { NULL }; // ??
    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&consumers[i], NULL, consumer_thread, NULL);
    }
    pthread_join(prod, NULL);
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(consumers[i], NULL);
    }
    pthread_cond_destroy(&condp);
    pthread_cond_destroy(&condc);
    pthread_mutex_destroy(&mutex);
    /*----------------------------------------------------------------*/

    double end_time = get_time();

    //if (nonce == 0) {
    if (global_nonce == 0) {
        printf("No solution found!\n");
        return 1;
    }

    /* When printed in hex, a SHA-1 checksum will be 40 characters. */
    char solution_hash[41];
    sha1tostring(solution_hash, digest);

    printf("Solution found by thread %d:\n", 0);
    //printf("Nonce: %lu\n", nonce);
    printf("Nonce: %lu\n", global_nonce);
    printf(" Hash: %s\n", solution_hash);

    double total_time = end_time - start_time;
    printf("%llu hashes in %.2fs (%.2f hashes/sec)\n",
            total_inversions, total_time, total_inversions / total_time);

    return 0;
}
