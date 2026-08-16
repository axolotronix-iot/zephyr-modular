#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

/* --- Configuración de la cola de mensajes --- */
#define MSGQ_MAX_MSGS   10
#define MSG_SIZE        sizeof(struct sensor_msg)

struct sensor_msg {
    uint32_t seq;
    int32_t  value;
};

K_MSGQ_DEFINE(sensor_msgq, MSG_SIZE, MSGQ_MAX_MSGS, 4);

/* --- Stacks y prioridades de los threads --- */
#define STACK_SIZE      1024
#define PRODUCER_PRIO   5
#define CONSUMER_PRIO   5

K_THREAD_STACK_DEFINE(producer_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(consumer_stack, STACK_SIZE);

static struct k_thread producer_tid;
static struct k_thread consumer_tid;

/* --- Thread productor: simula una lectura de sensor cada 500 ms --- */
void producer_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    uint32_t seq = 0;

    while (1) {
        struct sensor_msg msg = {
            .seq = seq++,
            .value = (int32_t)(sys_rand32_get() % 100),
        };

        int ret = k_msgq_put(&sensor_msgq, &msg, K_NO_WAIT);
        if (ret != 0) {
            printk("[producer] cola llena, mensaje %u descartado\n", msg.seq);
        } else {
            printk("[producer] enviado seq=%u value=%d\n", msg.seq, msg.value);
        }

        k_sleep(K_MSEC(500));
    }
}

/* --- Thread consumidor: procesa los mensajes conforme llegan --- */
void consumer_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sensor_msg msg;

    while (1) {
        k_msgq_get(&sensor_msgq, &msg, K_FOREVER);
        printk("[consumer] recibido seq=%u value=%d\n", msg.seq, msg.value);
    }
}

int main(void)
{
    k_thread_create(&producer_tid, producer_stack, STACK_SIZE,
                     producer_entry, NULL, NULL, NULL,
                     PRODUCER_PRIO, 0, K_NO_WAIT);

    k_thread_create(&consumer_tid, consumer_stack, STACK_SIZE,
                     consumer_entry, NULL, NULL, NULL,
                     CONSUMER_PRIO, 0, K_NO_WAIT);

    k_thread_name_set(&producer_tid, "producer");
    k_thread_name_set(&consumer_tid, "consumer");

    return 0;
}
