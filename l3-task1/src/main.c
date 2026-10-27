
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

/*
 * 0 = Baseline polling (10 ms)
 * 1 = Event-driven k_work
 * 2 = Debounce with k_work_reschedule (20 ms)
 */
#define APP_MODE 1

#define SENSOR_INTERVAL_MS 100
#define POLL_INTERVAL_MS    10
#define DEBOUNCE_MS         20
#define BURST_COUNT         5

/*
 * Shared event state.
 * The semaphore mirrors the starter architecture.
 */
K_SEM_DEFINE(sensor_sem, 0, 1);

static atomic_t sensor_flag;
static atomic_t sensor_events;
static atomic_t polling_wakeups;
static atomic_t work_executions;
static atomic_t debounce_events;

/* ---------------------------------------------------------
 * Event-driven work handler
 * --------------------------------------------------------- */
static void sensor_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int count = (int)atomic_inc(&work_executions) + 1;

    printk("[WORK] Handler executed: %d\n", count);
}

K_WORK_DEFINE(sensor_work, sensor_work_handler);

/* ---------------------------------------------------------
 * Debounce work handler
 * --------------------------------------------------------- */
static void debounce_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int count = (int)atomic_set(&debounce_events, 0);

    printk("[DEBOUNCE] Stable event burst: %d events\n", count);
}

K_WORK_DELAYABLE_DEFINE(debounce_work, debounce_work_handler);

/* ---------------------------------------------------------
 * Called when the simulated sensor generates an event
 * --------------------------------------------------------- */
static void sensor_event(void)
{
    atomic_set(&sensor_flag, 1);
    atomic_inc(&sensor_events);

    k_sem_give(&sensor_sem);

#if APP_MODE == 1
    k_work_submit(&sensor_work);

#elif APP_MODE == 2
    atomic_inc(&debounce_events);
    k_work_reschedule(&debounce_work, K_MSEC(DEBOUNCE_MS));
#endif
}

#if APP_MODE == 0

#define POLLER_STACK_SIZE 1024
#define POLLER_PRIORITY   5

K_THREAD_STACK_DEFINE(poller_stack, POLLER_STACK_SIZE);
static struct k_thread poller_thread;

static void polling_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    while (1) {
        /* Count each polling cycle / wake-up. */
        atomic_inc(&polling_wakeups);

        if (atomic_cas(&sensor_flag, 1, 0)) {
            printk("[POLL] Sensor event detected\n");
        }

        k_sleep(K_MSEC(POLL_INTERVAL_MS));
    }
}

#endif


#define SENSOR_STACK_SIZE 1024
#define SENSOR_PRIORITY   4

K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread;

static void sensor_sim(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

#if APP_MODE == 2
    /*
     * Bonus test: five events separated by 3 ms.
     * Total burst duration is approximately 12 ms.
     */
    printk("[SENSOR] Starting five-event debounce test\n");

    for (int i = 0; i < BURST_COUNT; i++) {
        sensor_event();
        k_sleep(K_MSEC(3));
    }
#endif

    while (1) {
        k_sleep(K_MSEC(SENSOR_INTERVAL_MS));

        sensor_event();

        printk("[SENSOR] Event generated: %d\n",
               (int)atomic_get(&sensor_events));
    }
}


int main(void)
{
    printk("\n=== L3 Task 1: Event-driven Workqueue ===\n");
    printk("APP_MODE = %d\n", APP_MODE);

#if APP_MODE == 0
    printk("Mode: 10 ms polling baseline\n");

    k_thread_create(&poller_thread,
                    poller_stack,
                    K_THREAD_STACK_SIZEOF(poller_stack),
                    polling_thread,
                    NULL, NULL, NULL,
                    POLLER_PRIORITY,
                    0,
                    K_NO_WAIT);

#elif APP_MODE == 1
    printk("Mode: event-driven k_work\n");

#elif APP_MODE == 2
    printk("Mode: 20 ms debounce\n");
#endif

    k_thread_create(&sensor_thread,
                    sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_sim,
                    NULL, NULL, NULL,
                    SENSOR_PRIORITY,
                    0,
                    K_NO_WAIT);

    while (1) {
        k_sleep(K_SECONDS(5));

#if APP_MODE == 0
        printk("[STATS] Polling cycles=%d, sensor events=%d\n",
               (int)atomic_get(&polling_wakeups),
               (int)atomic_get(&sensor_events));

#elif APP_MODE == 1
        printk("[STATS] Sensor events=%d, work executions=%d\n",
               (int)atomic_get(&sensor_events),
               (int)atomic_get(&work_executions));

#elif APP_MODE == 2
        printk("[STATS] Sensor events=%d, debounce pending count=%d\n",
               (int)atomic_get(&sensor_events),
               (int)atomic_get(&debounce_events));
#endif
    }

    return 0;
}
