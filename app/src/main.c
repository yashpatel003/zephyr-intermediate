#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(task1, LOG_LEVEL_INF);


// Thread stack size
#define THREAD_STACK_SIZE 2048

// Lower priority number means higher priority
#define T_LOW_PRIORITY   7
#define T_MED_PRIORITY   5
#define T_HIGH_PRIORITY  3
#define T_COOP_PRIORITY -1

//Sleep times for each priority
#define T_LOW_SLEEP_MS   300
#define T_MED_SLEEP_MS   200
#define T_HIGH_SLEEP_MS  100

// thread stacks
K_THREAD_STACK_DEFINE(t_low_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(t_med_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(t_high_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(t_coop_stack, THREAD_STACK_SIZE);

// thread data
struct k_thread t_low_data;
struct k_thread t_med_data;
struct k_thread t_high_data;
struct k_thread t_coop_data;


/*
 * LOW priority thread
 *
 * Priority: 7
 * Sleep:    300 ms
 */
void t_low_fn(void *p1, void *p2, void *p3)
{
    while (true) {
        LOG_INF("T_LOW running");

        k_sleep(K_MSEC(T_LOW_SLEEP_MS));
    }
}


/*
 * MEDIUM priority thread
 *
 * Priority: 5
 * Sleep:    200 ms
 */
void t_med_fn(void *p1, void *p2, void *p3)
{

    while (true) {
        LOG_INF("T_MED running");

        k_sleep(K_MSEC(T_MED_SLEEP_MS));
    }
}


/*
 * HIGH priority thread
 *
 * Priority: 3
 * Sleep:    100 ms
 */
void t_high_fn(void *p1, void *p2, void *p3)
{

    while (true) {
        LOG_INF("T_HIGH running");

        k_sleep(K_MSEC(T_HIGH_SLEEP_MS));
    }
}


// Cooperative thread - priority -1
// Do 5 busy-work iterations and then yield
void t_coop_fn(void *p1, void *p2, void *p3)
{
    while (true) {

        // do busy work 5 times
        for (int i = 0; i < 5; i++) {

            LOG_INF("T_COOP busy work %d/5", i + 1);

            // simple busy work
            volatile uint32_t work = 0;

            for (uint32_t j = 0; j < 1000; j++) {
                work += j;
            }
        }

        // give another ready thread a chance
        LOG_INF("T_COOP yielding");

        k_yield();
    }
}


// main function
int main(void)
{
    LOG_INF("Zephyr L1 Task 1 starting");

    //Create LOW priority thread
    k_thread_create(&t_low_data, t_low_stack, K_THREAD_STACK_SIZEOF(t_low_stack), t_low_fn, NULL, NULL, NULL, T_LOW_PRIORITY,0,K_NO_WAIT);
    

    //Create MEDIUM priority thread
    k_thread_create(&t_med_data,t_med_stack,K_THREAD_STACK_SIZEOF(t_med_stack),t_med_fn,NULL,NULL,NULL,T_MED_PRIORITY,0,K_NO_WAIT);

    //Create HIGH priority thread
    k_thread_create(&t_high_data,t_high_stack,K_THREAD_STACK_SIZEOF(t_high_stack),t_high_fn,NULL,NULL,NULL,T_HIGH_PRIORITY,0,K_NO_WAIT);

    // create cooperative thread
    k_thread_create(&t_coop_data,t_coop_stack,K_THREAD_STACK_SIZEOF(t_coop_stack),t_coop_fn,NULL,NULL,NULL,T_COOP_PRIORITY,0,K_NO_WAIT
    );

    LOG_INF("Four threads created");

    //Keep the main thread alive.
    while (true) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}