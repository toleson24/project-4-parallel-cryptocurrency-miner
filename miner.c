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
long int condition = 0;
char *global_bitcoin_block_data;
uint32_t global_difficulty_mask = 0x0;
uint64_t global_nonce = 0;
/* When printed in hex, a SHA-1 checksum will be 40 characters. */
char global_solution_hash[41];

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

void *consumer_thread(void *ptr) {
    //int *local_id = malloc(sizeof(int));
    //local_id = *(int *)ptr; 
    while (true) {
        long int local;
        uint8_t digest[SHA1_HASH_SIZE];
        pthread_mutex_lock(&mutex);
        while (condition == 0 && global_nonce == 0) {
            pthread_cond_wait(&condc, &mutex);
        }
        if (global_nonce != 0) {
            pthread_cond_signal(&condp);
            pthread_mutex_unlock(&mutex);
            //free(ptr);
            pthread_exit(0);
            //pthread_exit(local_id);
        }
        local = condition;
        condition = 0;
        pthread_cond_signal(&condp);
        pthread_mutex_unlock(&mutex);
        uint64_t nonce = mine(
                global_bitcoin_block_data,
                global_difficulty_mask,
                local, local + 100,
                digest
        );
        if (nonce != 0) {
            pthread_mutex_lock(&mutex);
            global_nonce = nonce;
            sha1tostring(global_solution_hash, digest);
            pthread_cond_broadcast(&condc);
            pthread_cond_signal(&condp);
            pthread_mutex_unlock(&mutex);
            //free(ptr);
            pthread_exit(0);
            //pthread_exit(local_id); // 0
        }
    }
    return 0;
    //return local_id; // 0
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Usage: %s threads difficulty 'block data (string)'\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[1]);
    if (num_threads < 1) {
        return EXIT_FAILURE;
    }
    printf("Number of threads: %d\n", num_threads);

    int difficulty = atoi(argv[2]);
    printf("Number of leading 0s: %d\n", difficulty);

    for (int i = 0; i < 32 - difficulty; ++i) {
        global_difficulty_mask = global_difficulty_mask | 1 << i;
    }
    printf("  Difficulty Mask: ");
    print_binary32(global_difficulty_mask);
    
    global_bitcoin_block_data = argv[3];
    printf("       Block data: [%s]\n", global_bitcoin_block_data);

    printf("\n----------- Starting up miner threads!  -----------\n\n");

    double start_time = get_time();

    /*----------------------------------------------------------------*/
    pthread_t consumers[num_threads];    
    for (int i = 0; i < num_threads; ++i) {
        //int *thread_id = malloc(sizeof(int));
        //*thread_id = i;
        pthread_create(consumers + i, NULL, consumer_thread, NULL);
        //pthread_create(consumers + i, NULL, consumer_thread, thread_id);
    }
    /* Producer */
    int start = 1;
    while (true) {
        pthread_mutex_lock(&mutex);
        while (condition != 0 && global_nonce == 0) {  
            pthread_cond_wait(&condp, &mutex);
        }
        if (global_nonce != 0) {
            pthread_cond_signal(&condc);
            pthread_mutex_unlock(&mutex); 
            break;
        }
        start += 100;
        condition = start;
        pthread_cond_signal(&condc);
        pthread_mutex_unlock(&mutex);  
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(consumers[i], NULL);
    }
    pthread_cond_destroy(&condp);
    pthread_cond_destroy(&condc);
    pthread_mutex_destroy(&mutex);
    /*----------------------------------------------------------------*/

    double end_time = get_time();

    if (global_nonce == 0) {
        printf("No solution found!\n");
        return 1;
    }

    printf("Solution found by thread %d:\n", 0); // TODO!
    printf("Nonce: %lu\n", global_nonce); 
    printf(" Hash: %s\n", global_solution_hash);

    double total_time = end_time - start_time;
    printf("%llu hashes in %.2fs (%.2f hashes/sec)\n",
            total_inversions, total_time, total_inversions / total_time);

    return 0;
}
