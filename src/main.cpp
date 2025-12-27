#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "solder_test";

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
    printf("\n=== Checking for shorts to GND ===\n");

    for (int i = 0; i < NUM_ALL_PINS; i++) {
        gpio_num_t pin = ALL_PINS[i];
        configure_pin_input_pullup(pin);
        delay_ms(10);

        if (gpio_get_level(pin) == 0) {
            printf("WARNING: IO%d appears shorted to GND!\n", pin);
            shorts_detected = true;
        }
    }

    if (!shorts_detected) {
        printf("No shorts to GND detected.\n");
    }
}

static void detect_shorts_between_pins(void) {
    printf("\n=== Checking for shorts between pins ===\n");

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
                printf("WARNING: IO%d and IO%d appear shorted together!\n", test_pin, check_pin);
                shorts_detected = true;
            }
        }

        // Restore to INPUT_PULLUP
        configure_pin_input_pullup(test_pin);
        delay_ms(5);
    }

    if (!shorts_detected) {
        printf("No shorts between pins detected.\n");
    }
}

static void test_pin(gpio_num_t pin) {
    printf("\n>>> Testing IO%d - Connect LED now, press button when ready...\n", pin);

    // Wait for button press to start
    wait_for_button_press();

    printf("Blinking IO%d - Press button to continue to next pin...\n", pin);

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

    printf("IO%d test complete.\n", pin);
}

static void test_button_pin(void) {
    printf("\n>>> Final test: IO0 (button pin)\n");
    printf("1. Move button to a different pin (e.g., IO1)\n");
    printf("2. Connect LED to IO0\n");
    printf("3. Press the relocated button when ready...\n");

    // Reconfigure: IO1 becomes button, IO0 becomes test pin
    configure_pin_input_pullup(GPIO_NUM_1);

    // Wait for new button press on IO1
    while (gpio_get_level(GPIO_NUM_1) == 1) {
        delay_ms(10);
    }
    delay_ms(50);

    printf("Blinking IO0 - Press button (on IO1) to finish...\n");

    configure_pin_output(BUTTON_PIN);

    while (gpio_get_level(GPIO_NUM_1) == 1) {
        gpio_set_level(BUTTON_PIN, 1);
        delay_ms(200);
        gpio_set_level(BUTTON_PIN, 0);
        delay_ms(200);
    }

    configure_pin_input_pullup(BUTTON_PIN);
    printf("IO0 test complete.\n");
}

extern "C" void app_main(void) {
    delay_ms(2000); // Wait for serial monitor

    // Setup onboard LED
    configure_pin_output(ONBOARD_LED);

    // Setup button with internal pull-up
    configure_pin_input_pullup(BUTTON_PIN);

    printf("\n========================================\n");
    printf("  ESP32-C6 Super Mini Solder Test\n");
    printf("========================================\n");
    printf("\nHardware setup:\n");
    printf("- Button: IO0 to GND\n");
    printf("- LED: One leg to GND, other leg free\n");

    // Blink onboard LED to confirm boot
    printf("\nBlinking onboard LED (IO15) to confirm boot...\n");
    blink_onboard_led(3, 200);
    printf("Boot OK! Serial working = TX/RX OK!\n");

    // Run short detection
    printf("\n--- PHASE 1: Short Detection ---\n");
    detect_shorts_to_ground();
    detect_shorts_between_pins();

    if (shorts_detected) {
        printf("\n!!! SHORTS DETECTED !!!\n");
        printf("Fix soldering issues before continuing.\n");
        printf("Press button to continue anyway (not recommended)...\n");
        wait_for_button_press();
    }

    // Interactive pin testing
    printf("\n--- PHASE 2: Interactive Pin Testing ---\n");
    printf("Testing %d GPIO pins...\n", NUM_TEST_PINS);

    for (int i = 0; i < NUM_TEST_PINS; i++) {
        test_pin(TEST_PINS[i]);
    }

    // Test button pin last
    test_button_pin();

    // Done!
    printf("\n========================================\n");
    printf("  ALL TESTS COMPLETE!\n");
    printf("========================================\n");
    printf("\nIf all pins blinked the LED, your soldering is good!\n");

    // Victory blink
    blink_onboard_led(5, 100);

    // Keep running
    while (1) {
        delay_ms(1000);
    }
}
