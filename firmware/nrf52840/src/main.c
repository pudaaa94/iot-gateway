#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#define COMPANY_ID_LOW  0xFF
#define COMPANY_ID_HIGH 0xFF

/* Buffer za manufacturer specific data:
 * [company_id 2B][temp_int 2B][temp_dec 2B][press_int 2B]
 */
static uint8_t manuf_data[4] = {
    COMPANY_ID_LOW, COMPANY_ID_HIGH,
    0, 0,  /* temp u 0.01°C, int16 */
};

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, manuf_data, sizeof(manuf_data)),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, 'n', 'R', 'F', 'S', 'e', 'n', 's', 'o', 'r'),
};

static void update_adv_data(struct sensor_value *temp)
{
    int16_t t = (int16_t)(temp->val1 * 100 + temp->val2 / 10000);
    manuf_data[2] = (t >> 8) & 0xFF;
    manuf_data[3] = t & 0xFF;
    bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

int main(void)
{
    int err;

    /* Inicijalizacija senzora */
    const struct device *dev = DEVICE_DT_GET_ANY(bosch_bme280);
    if (!device_is_ready(dev)) {
        printk("Senzor nije dostupan\n");
        return -1;
    }
    printk("Senzor OK\n");

    /* Inicijalizacija BLE */
    err = bt_enable(NULL);
    if (err) {
        printk("BLE init failed (err %d)\n", err);
        return -1;
    }
    printk("BLE OK\n");

    /* Start advertising */
    err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed (err %d)\n", err);
        return -1;
    }
    printk("Advertising started\n");

    while (1) {
        struct sensor_value temp;

        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		printk("Temp: %d.%02d C\n", temp.val1, temp.val2 / 10000);
		update_adv_data(&temp);

        k_sleep(K_MSEC(2000));
    }

    return 0;
}