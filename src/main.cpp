#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"

// Button pin (directly to GND, uses internal pull-up)
#define BUTTON_PIN GPIO_NUM_0

// Onboard LED
#define ONBOARD_LED GPIO_NUM_15

// All GPIO pins to test (excluding button pin IO0, tested last)
static const gpio_num_t TEST_PINS[] = {
    GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5,
    GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_14,
    GPIO_NUM_15, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_20
};
#define NUM_TEST_PINS (sizeof(TEST_PINS) / sizeof(TEST_PINS[0]))

// All GPIO pins including button for short detection
static const gpio_num_t ALL_PINS[] = {
    GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5,
    GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_14,
    GPIO_NUM_15, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_20
};
#define NUM_ALL_PINS (sizeof(ALL_PINS) / sizeof(ALL_PINS[0]))

static bool shorts_detected = false;

static void delay_ms(int ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void usb_print(const char* msg) {
    usb_serial_jtag_write_bytes((const uint8_t*)msg, strlen(msg), pdMS_TO_TICKS(100));
}

static void usb_printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    usb_print(buf);
}

static void blink_onboard_led(int times, int delay_time) {
    for (int i = 0; i < times; i++) {
        gpio_set_level(ONBOARD_LED, 1);
        delay_ms(delay_time);
        gpio_set_level(ONBOARD_LED, 0);
        delay_ms(delay_time);
    }
}

static void wait_for_button_press(void) {
    // Wait for button release first (in case held)
    while (gpio_get_level(BUTTON_PIN) == 0) {
        delay_ms(10);
    }
    delay_ms(50); // Debounce

    // Wait for button press
    while (gpio_get_level(BUTTON_PIN) == 1) {
        delay_ms(10);
    }
    delay_ms(50); // Debounce
}

static void configure_pin_input_pullup(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

static void configure_pin_output(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

static void detect_shorts_to_ground(void) {
    usb_print("\r\n=== Checking for shorts to GND ===\r\n");

    for (int i = 0; i < NUM_ALL_PINS; i++) {
        gpio_num_t pin = ALL_PINS[i];
        configure_pin_input_pullup(pin);
        delay_ms(10);

        if (gpio_get_level(pin) == 0) {
            usb_printf("WARNING: IO%d appears shorted to GND!\r\n", pin);
            shorts_detected = true;
        }
    }

    if (!shorts_detected) {
        usb_print("No shorts to GND detected.\r\n");
    }
}

static void detect_shorts_between_pins(void) {
    usb_print("\r\n=== Checking for shorts between pins ===\r\n");

    // Set all pins as INPUT_PULLUP first
    for (int i = 0; i < NUM_ALL_PINS; i++) {
        configure_pin_input_pullup(ALL_PINS[i]);
    }
    delay_ms(10);

    // Test each pin
    for (int i = 0; i < NUM_ALL_PINS; i++) {
        gpio_num_t test_pin = ALL_PINS[i];

        // Set test pin LOW
        configure_pin_output(test_pin);
        gpio_set_level(test_pin, 0);
        delay_ms(5);

        // Check all other pins
        for (int j = 0; j < NUM_ALL_PINS; j++) {
            if (i == j) continue;

            gpio_num_t check_pin = ALL_PINS[j];
            if (gpio_get_level(check_pin) == 0) {
                usb_printf("WARNING: IO%d and IO%d appear shorted together!\r\n", test_pin, check_pin);
                shorts_detected = true;
            }
        }

        // Restore to INPUT_PULLUP
        configure_pin_input_pullup(test_pin);
        delay_ms(5);
    }

    if (!shorts_detected) {
        usb_print("No shorts between pins detected.\r\n");
    }
}

static void test_pin(gpio_num_t pin) {
    usb_printf("\r\n>>> Testing IO%d - Connect LED now, press button when ready...\r\n", pin);

    // Wait for button press to start
    wait_for_button_press();

    usb_printf("Blinking IO%d - Press button to continue to next pin...\r\n", pin);

    configure_pin_output(pin);

    // Blink until button pressed
    while (gpio_get_level(BUTTON_PIN) == 1) {
        gpio_set_level(pin, 1);
        delay_ms(200);
        gpio_set_level(pin, 0);
        delay_ms(200);
    }

    // Cleanup
    configure_pin_input_pullup(pin);
    delay_ms(50); // Debounce

    usb_printf("IO%d test complete.\r\n", pin);
}

static void test_button_pin(void) {
    usb_print("\r\n>>> Final test: IO0 (button pin)\r\n");
    usb_print("1. Move button to a different pin (e.g., IO1)\r\n");
    usb_print("2. Connect LED to IO0\r\n");
    usb_print("3. Press the relocated button when ready...\r\n");

    // Reconfigure: IO1 becomes button, IO0 becomes test pin
    configure_pin_input_pullup(GPIO_NUM_1);

    // Wait for new button press on IO1
    while (gpio_get_level(GPIO_NUM_1) == 1) {
        delay_ms(10);
    }
    delay_ms(50);

    usb_print("Blinking IO0 - Press button (on IO1) to finish...\r\n");

    configure_pin_output(BUTTON_PIN);

    while (gpio_get_level(GPIO_NUM_1) == 1) {
        gpio_set_level(BUTTON_PIN, 1);
        delay_ms(200);
        gpio_set_level(BUTTON_PIN, 0);
        delay_ms(200);
    }

    configure_pin_input_pullup(BUTTON_PIN);
    usb_print("IO0 test complete.\r\n");
}

extern "C" void app_main(void) {
    // Initialize USB Serial JTAG first
    usb_serial_jtag_driver_config_t usb_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    esp_vfs_usb_serial_jtag_use_driver();

    // Setup onboard LED
    configure_pin_output(ONBOARD_LED);

    // Setup button with internal pull-up
    configure_pin_input_pullup(BUTTON_PIN);

    usb_print("\r\n========================================\r\n");
    usb_print("  ESP32-C6 Super Mini Solder Test\r\n");
    usb_print("========================================\r\n");
    usb_print("\r\nHardware setup:\r\n");
    usb_print("- Button: IO0 to GND\r\n");
    usb_print("- LED: One leg to GND, other leg free\r\n");

    // Blink onboard LED to confirm boot
    usb_print("\r\nBlinking onboard LED (IO15) to confirm boot...\r\n");
    blink_onboard_led(3, 200);
    usb_print("Boot OK! Serial working = TX/RX OK!\r\n");

    // Run short detection
    usb_print("\r\n--- PHASE 1: Short Detection ---\r\n");
    detect_shorts_to_ground();
    detect_shorts_between_pins();

    if (shorts_detected) {
        usb_print("\r\n!!! SHORTS DETECTED !!!\r\n");
        usb_print("Fix soldering issues before continuing.\r\n");
        usb_print("Press button to continue anyway (not recommended)...\r\n");
        wait_for_button_press();
    }

    // Interactive pin testing
    usb_print("\r\n--- PHASE 2: Interactive Pin Testing ---\r\n");
    usb_printf("Testing %d GPIO pins...\r\n", NUM_TEST_PINS);

    for (int i = 0; i < NUM_TEST_PINS; i++) {
        test_pin(TEST_PINS[i]);
    }

    // Test button pin last
    test_button_pin();

    // Done!
    usb_print("\r\n========================================\r\n");
    usb_print("  ALL TESTS COMPLETE!\r\n");
    usb_print("========================================\r\n");
    usb_print("\r\nIf all pins blinked the LED, your soldering is good!\r\n");

    // Victory blink
    blink_onboard_led(5, 100);

    // Keep running
    while (1) {
        delay_ms(1000);
    }
}
