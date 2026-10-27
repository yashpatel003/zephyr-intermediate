
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/sys/printk.h>

/* Sensor publication period */
#define SENSOR_PERIOD_MS 100

/* Print one slow log for every five sensor samples */
#define LOG_EVERY_N_SAMPLES 5

struct sensor_data {
    int sample_id;
    int temperature_c;
    int humidity_pct;
};

/* Forward declaration for the listener callback */
static void display_listener_cb(const struct zbus_channel *chan);

/* Fast, synchronous display listener */
ZBUS_LISTENER_DEFINE(display_listener, display_listener_cb);

/* Subscriber with a queue for pending channel notifications */
ZBUS_SUBSCRIBER_DEFINE(logging_subscriber, 10);


ZBUS_CHAN_DEFINE(
    sensor_data_chan,
    struct sensor_data,
    NULL,
    NULL,
    ZBUS_OBSERVERS(display_listener, logging_subscriber),
    ZBUS_MSG_INIT(
        .sample_id = 0,
        .temperature_c = 25,
        .humidity_pct = 50
    )
);


static void display_listener_cb(const struct zbus_channel *chan)
{
    if (chan != &sensor_data_chan) {
        return;
    }

    const struct sensor_data *data = zbus_chan_const_msg(chan);

    printk("[DISPLAY] Sample %d | Temp=%d C | Humidity=%d%%\n",
           data->sample_id,
           data->temperature_c,
           data->humidity_pct);
}


static void sensor_producer(void)
{
    struct sensor_data data = {
        .sample_id = 0,
        .temperature_c = 25,
        .humidity_pct = 50
    };

    printk("[SENSOR] Producer started\n");

    while (1) {
        data.sample_id++;

        /* Simulate changing sensor values */
        data.temperature_c = 25 + (data.sample_id % 6);
        data.humidity_pct = 45 + (data.sample_id % 16);

        int err = zbus_chan_pub(
            &sensor_data_chan,
            &data,
            K_MSEC(50)
        );

        if (err != 0) {
            printk("[SENSOR] Publish failed: %d\n", err);
        }

        k_sleep(K_MSEC(SENSOR_PERIOD_MS));
    }
}


static void logging_thread(void)
{
    const struct zbus_channel *chan;
    struct sensor_data data;
    int notification_count = 0;

    printk("[LOGGER] Subscriber started\n");

    /* Associate the observer with this thread. */
    zbus_obs_attach_to_thread(&logging_subscriber);

    while (1) {
        int err = zbus_sub_wait(
            &logging_subscriber,
            &chan,
            K_FOREVER
        );

        if (err != 0) {
            printk("[LOGGER] Wait failed: %d\n", err);
            continue;
        }

        if (chan != &sensor_data_chan) {
            continue;
        }

        notification_count++;

        /*
         * Drain every notification, but only log every
         * fifth one to keep the logging rate lower.
         */
        if ((notification_count % LOG_EVERY_N_SAMPLES) == 0) {
            err = zbus_chan_read(chan, &data, K_MSEC(50));

            if (err == 0) {
                printk("[LOGGER] Sample %d | Temp=%d C | Humidity=%d%%\n",
                       data.sample_id,
                       data.temperature_c,
                       data.humidity_pct);
            } else {
                printk("[LOGGER] Read failed: %d\n", err);
            }
        }
    }
}


#define PRODUCER_STACK_SIZE 1024
#define LOGGER_STACK_SIZE   1536

#define PRODUCER_PRIORITY 5
#define LOGGER_PRIORITY   6

K_THREAD_STACK_DEFINE(producer_stack, PRODUCER_STACK_SIZE);
K_THREAD_STACK_DEFINE(logger_stack, LOGGER_STACK_SIZE);

static struct k_thread producer_thread_data;
static struct k_thread logger_thread_data;

int main(void)
{
    printk("\n=== L4 Task 1: Zephyr zbus ===\n");
    printk("One channel, one listener, one subscriber\n");

    k_thread_create(
        &logger_thread_data,
        logger_stack,
        K_THREAD_STACK_SIZEOF(logger_stack),
        (k_thread_entry_t)logging_thread,
        NULL, NULL, NULL,
        LOGGER_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_create(
        &producer_thread_data,
        producer_stack,
        K_THREAD_STACK_SIZEOF(producer_stack),
        (k_thread_entry_t)sensor_producer,
        NULL, NULL, NULL,
        PRODUCER_PRIORITY,
        0,
        K_MSEC(100)
    );

    return 0;
}
