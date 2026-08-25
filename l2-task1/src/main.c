#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(l2_task1, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE       1024
#define THREAD_PRIORITY         5

#define NUM_THREADS             2
#define INCREMENTS_PER_THREAD   100000
#define LOG_INTERVAL            25000

// Thread stacks
K_THREAD_STACK_DEFINE(thread1_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread2_stack, THREAD_STACK_SIZE);

// Thread data
static struct k_thread thread1_data;
static struct k_thread thread2_data;

// Shared counter
static int shared_counter;

// Mutex for protecting shared_counter
static struct k_mutex counter_mutex;


// Thread 1
static void thread1_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Thread 1 started, priority %d", THREAD_PRIORITY);

    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        shared_counter++;

        k_mutex_unlock(&counter_mutex);

        if ((i + 1) % LOG_INTERVAL == 0) {
            LOG_INF("Thread 1: %d increments, counter = %d",
                    i + 1, shared_counter);
        }
    }

    LOG_INF("Thread 1 finished");
}


// Thread 2
static void thread2_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Thread 2 started, priority %d", THREAD_PRIORITY);

    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        shared_counter++;

        k_mutex_unlock(&counter_mutex);

        if ((i + 1) % LOG_INTERVAL == 0) {
            LOG_INF("Thread 2: %d increments, counter = %d",
                    i + 1, shared_counter);
        }
    }

    LOG_INF("Thread 2 finished");
}


int main(void)
{
    int expected_counter =
        NUM_THREADS * INCREMENTS_PER_THREAD;

    LOG_INF("L2 Task 1");
    LOG_INF("Two equal-priority threads");
    LOG_INF("Expected counter: %d", expected_counter);

    // Start with a clean counter
    shared_counter = 0;

    // Initialize the mutex
    k_mutex_init(&counter_mutex);

    LOG_INF("Starting threads at priority %d", THREAD_PRIORITY);

    // Create thread 1
    k_thread_create(
        &thread1_data,
        thread1_stack,
        K_THREAD_STACK_SIZEOF(thread1_stack),
        thread1_fn,
        NULL,
        NULL,
        NULL,
        THREAD_PRIORITY,
        0,
        K_NO_WAIT
    );

    // Create thread 2
    k_thread_create(
        &thread2_data,
        thread2_stack,
        K_THREAD_STACK_SIZEOF(thread2_stack),
        thread2_fn,
        NULL,
        NULL,
        NULL,
        THREAD_PRIORITY,
        0,
        K_NO_WAIT
    );

    // Wait for both threads to finish
    k_thread_join(&thread1_data, K_FOREVER);
    k_thread_join(&thread2_data, K_FOREVER);

    LOG_INF("------------------------------");
    LOG_INF("Final counter: %d", shared_counter);
    LOG_INF("Expected counter: %d", expected_counter);

    if (shared_counter == expected_counter) {
        LOG_INF("Counter is correct - mutex fixed the race");
    } else {
        LOG_ERR("Counter is incorrect");
        LOG_ERR("Race condition is still present");
    }

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}