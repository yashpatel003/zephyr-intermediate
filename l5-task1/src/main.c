
#include <zephyr/kernel.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

/* ---------------- Configuration ---------------- */

#define QUEUE_CAPACITY       8
#define PRODUCER_PERIOD_MS   100
#define CONSUMER_STALL_MS    5000
#define WATCHDOG_TIMEOUT_MS  1500
#define HEALTH_PERIOD_MS     200
#define HEALTH_THRESHOLD     75

struct sensor_sample {
    uint32_t sequence;
    int32_t value;
};

K_MSGQ_DEFINE(sample_queue,
              sizeof(struct sensor_sample),
              QUEUE_CAPACITY,
              4);

static atomic_t produced_count;
static atomic_t consumed_count;
static atomic_t dropped_count;
static atomic_t watchdog_fired;

/* ---------------- Watchdog callback ---------------- */

static void consumer_wdt_callback(int channel_id, void *user_data)
{
    ARG_UNUSED(user_data);

    atomic_set(&watchdog_fired, 1);

    printk("\n[WATCHDOG] Consumer timeout! Channel=%d\n",
           channel_id);
    printk("[WATCHDOG] Consumer failed to report progress in time.\n");
}

/* ---------------- Producer thread ---------------- */

static void producer_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sensor_sample sample = {
        .sequence = 0,
        .value = 0
    };

    while (1) {
        sample.sequence++;
        sample.value = (int32_t)(sample.sequence % 100);

        int err = k_msgq_put(&sample_queue, &sample, K_NO_WAIT);

        if (err == 0) {
            atomic_inc(&produced_count);
        } else {
            atomic_inc(&dropped_count);
            printk("[PRODUCER] Queue FULL! Dropped sample %u\n",
                   sample.sequence);
        }

        k_sleep(K_MSEC(PRODUCER_PERIOD_MS));
    }
}

/* ---------------- Consumer thread ---------------- */

static void consumer_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sensor_sample sample;

    /*
     * Register the consumer's watchdog channel.
     * A timeout means the consumer has not fed it
     * within the configured deadline.
     */
    int wdt_channel = task_wdt_add(
        WATCHDOG_TIMEOUT_MS,
        consumer_wdt_callback,
        NULL
    );

    if (wdt_channel < 0) {
        printk("[CONSUMER] Failed to add watchdog: %d\n",
               wdt_channel);
        return;
    }

    printk("[CONSUMER] Started. Watchdog channel=%d\n",
           wdt_channel);

    while (1) {
        int err = k_msgq_get(&sample_queue, &sample, K_FOREVER);

        if (err != 0) {
            continue;
        }

        printk("[CONSUMER] Received sample %u; simulating stall...\n",
               sample.sequence);

        /*
         * Intentional reliability fault:
         * the consumer stops processing for 5 seconds.
         */
        k_sleep(K_MSEC(CONSUMER_STALL_MS));

        atomic_inc(&consumed_count);

        printk("[CONSUMER] Finished stalled processing of %u\n",
               sample.sequence);

        /*
         * Feed only after completing processing.
         * The watchdog should already have expired during
         * the intentional 5-second stall.
         */
        err = task_wdt_feed(wdt_channel);

        if (err != 0) {
            printk("[CONSUMER] Watchdog feed failed: %d\n", err);
        }
    }
}

/* ---------------- Health-check thread ---------------- */

static void health_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    bool warning_active = false;

    while (1) {
        uint32_t used = k_msgq_num_used_get(&sample_queue);
        uint32_t percent = (used * 100U) / QUEUE_CAPACITY;

        if (percent >= HEALTH_THRESHOLD) {
            if (!warning_active) {
                printk("[HEALTH] WARNING: Queue at %u%% (%u/%u)\n",
                       percent, used, QUEUE_CAPACITY);
                warning_active = true;
            }
        } else {
            warning_active = false;
        }

        k_sleep(K_MSEC(HEALTH_PERIOD_MS));
    }
}

/* ---------------- Thread definitions ---------------- */

#define PRODUCER_STACK_SIZE 1024
#define CONSUMER_STACK_SIZE 1536
#define HEALTH_STACK_SIZE   1024

#define PRODUCER_PRIORITY 5
#define CONSUMER_PRIORITY 5
#define HEALTH_PRIORITY   6

K_THREAD_STACK_DEFINE(producer_stack, PRODUCER_STACK_SIZE);
K_THREAD_STACK_DEFINE(consumer_stack, CONSUMER_STACK_SIZE);
K_THREAD_STACK_DEFINE(health_stack, HEALTH_STACK_SIZE);

static struct k_thread producer_data;
static struct k_thread consumer_data;
static struct k_thread health_data;

/* ---------------- Main ---------------- */

int main(void)
{
    printk("\n=== L5 Task 1: Reliability Under Pressure ===\n");
    printk("Queue capacity: %d\n", QUEUE_CAPACITY);
    printk("Producer period: %d ms\n", PRODUCER_PERIOD_MS);
    printk("Consumer stall: %d ms\n", CONSUMER_STALL_MS);
    printk("Watchdog timeout: %d ms\n", WATCHDOG_TIMEOUT_MS);

    int err = task_wdt_init(NULL);

    if (err != 0) {
        printk("[MAIN] task_wdt_init failed: %d\n", err);
        return 0;
    }

    k_thread_create(
        &consumer_data,
        consumer_stack,
        K_THREAD_STACK_SIZEOF(consumer_stack),
        consumer_thread,
        NULL, NULL, NULL,
        CONSUMER_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_create(
        &producer_data,
        producer_stack,
        K_THREAD_STACK_SIZEOF(producer_stack),
        producer_thread,
        NULL, NULL, NULL,
        PRODUCER_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_create(
        &health_data,
        health_stack,
        K_THREAD_STACK_SIZEOF(health_stack),
        health_thread,
        NULL, NULL, NULL,
        HEALTH_PRIORITY,
        0,
        K_NO_WAIT
    );

    while (1) {
        k_sleep(K_SECONDS(2));

        printk("[STATS] Produced=%d Consumed=%d Dropped=%d Queue=%u/%d WDT=%d\n",
               (int)atomic_get(&produced_count),
               (int)atomic_get(&consumed_count),
               (int)atomic_get(&dropped_count),
               k_msgq_num_used_get(&sample_queue),
               QUEUE_CAPACITY,
               (int)atomic_get(&watchdog_fired));
    }

    return 0;
}
