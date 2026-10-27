
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

#define CORRECTED_RUN          0

#define QUEUE_CAPACITY         8
#define PRODUCER_PERIOD_MS     20
#define CONSUMER_PROCESS_MS    5
#define RESPONSE_DEADLINE_MS   50
#define REPORT_PERIOD_MS       2000

#if CORRECTED_RUN
#define PRODUCER_PRIORITY      5
#define CONSUMER_PRIORITY      3
#else
#define PRODUCER_PRIORITY      3
#define CONSUMER_PRIORITY      5
#endif


struct sample {
    uint32_t sequence;
    int32_t value;
    int64_t release_ms;
};

K_MSGQ_DEFINE(sample_queue,
              sizeof(struct sample),
              QUEUE_CAPACITY,
              4);


static atomic_t produced_count;
static atomic_t completed_count;
static atomic_t deadline_miss_count;
static atomic_t dropped_count;

static int64_t maximum_response_ms;


#define PRODUCER_STACK_SIZE 1024

K_THREAD_STACK_DEFINE(producer_stack, PRODUCER_STACK_SIZE);
static struct k_thread producer_thread_data;

static void producer_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    uint32_t sequence = 0;

    while (1) {
        struct sample item;

        item.sequence = ++sequence;
        item.value = (int32_t)(sequence % 100);
        item.release_ms = k_uptime_get();

        int err = k_msgq_put(&sample_queue, &item, K_NO_WAIT);

        if (err == 0) {
            atomic_inc(&produced_count);

            printk("[PROD] seq=%u released=%lld ms\n",
                   item.sequence,
                   (long long)item.release_ms);
        } else {
            atomic_inc(&dropped_count);

            printk("[DROP] seq=%u queue full\n",
                   item.sequence);
        }

        k_sleep(K_MSEC(PRODUCER_PERIOD_MS));
    }
}


#define CONSUMER_STACK_SIZE 1536

K_THREAD_STACK_DEFINE(consumer_stack, CONSUMER_STACK_SIZE);
static struct k_thread consumer_thread_data;

static void consumer_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sample item;

    while (1) {
        int err = k_msgq_get(&sample_queue, &item, K_FOREVER);

        if (err != 0) {
            continue;
        }

        /* Simulate a bounded processing workload. */
        k_sleep(K_MSEC(CONSUMER_PROCESS_MS));

        int64_t completed_ms = k_uptime_get();
        int64_t response_ms = completed_ms - item.release_ms;

        if (response_ms > maximum_response_ms) {
            maximum_response_ms = response_ms;
        }

        atomic_inc(&completed_count);

        if (response_ms > RESPONSE_DEADLINE_MS) {
            atomic_inc(&deadline_miss_count);

            printk("[MISS] seq=%u response=%lld ms deadline=%d ms\n",
                   item.sequence,
                   (long long)response_ms,
                   RESPONSE_DEADLINE_MS);
        } else {
            printk("[DONE] seq=%u response=%lld ms\n",
                   item.sequence,
                   (long long)response_ms);
        }
    }
}


int main(void)
{
    printk("\n=== L6 Task 1: Scheduling Delay ===\n");

#if CORRECTED_RUN
    printk("Run: CORRECTED (consumer has higher priority)\n");
#else
    printk("Run: BASELINE (producer has higher priority)\n");
#endif

    printk("Producer priority=%d, consumer priority=%d\n",
           PRODUCER_PRIORITY, CONSUMER_PRIORITY);

    printk("Deadline=%d ms, producer period=%d ms\n",
           RESPONSE_DEADLINE_MS, PRODUCER_PERIOD_MS);

    k_thread_create(
        &consumer_thread_data,
        consumer_stack,
        K_THREAD_STACK_SIZEOF(consumer_stack),
        consumer_entry,
        NULL, NULL, NULL,
        CONSUMER_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_name_set(&consumer_thread_data, "consumer");

    k_thread_create(
        &producer_thread_data,
        producer_stack,
        K_THREAD_STACK_SIZEOF(producer_stack),
        producer_entry,
        NULL, NULL, NULL,
        PRODUCER_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_name_set(&producer_thread_data, "producer");

    while (1) {
        k_sleep(K_MSEC(REPORT_PERIOD_MS));

        printk("\n[STATS] produced=%d completed=%d dropped=%d misses=%d queue=%u/%d max_response=%lld ms\n",
               (int)atomic_get(&produced_count),
               (int)atomic_get(&completed_count),
               (int)atomic_get(&dropped_count),
               (int)atomic_get(&deadline_miss_count),
               (unsigned int)k_msgq_num_used_get(&sample_queue),
               QUEUE_CAPACITY,
               (long long)maximum_response_ms);
    }

    return 0;
}
