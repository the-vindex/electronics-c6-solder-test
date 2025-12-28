#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"

#define ONBOARD_LED GPIO_NUM_15

extern "C" void app_main(void) {
    // Configure LED first so we can see if code runs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ONBOARD_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Quick blink to show we're starting USB init
    gpio_set_level(ONBOARD_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ONBOARD_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize USB Serial JTAG VFS
    usb_serial_jtag_driver_config_t usb_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));

    // Connect VFS to USB Serial JTAG driver
    esp_vfs_usb_serial_jtag_use_driver();

    // Double blink to show USB init complete
    gpio_set_level(ONBOARD_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ONBOARD_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ONBOARD_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ONBOARD_LED, 0);

    int count = 0;
    char buf[64];
    while (1) {
        gpio_set_level(ONBOARD_LED, 1);
        int len = snprintf(buf, sizeof(buf), "LED ON - count %d\r\n", count);
        usb_serial_jtag_write_bytes((const uint8_t*)buf, len, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(500));

        gpio_set_level(ONBOARD_LED, 0);
        len = snprintf(buf, sizeof(buf), "LED OFF - count %d\r\n", count);
        usb_serial_jtag_write_bytes((const uint8_t*)buf, len, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(500));

        count++;
    }
}
