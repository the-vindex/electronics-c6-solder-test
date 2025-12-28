# ESP32-C6 Super Mini Solder Test

Test firmware to verify soldered header pins on ESP32-C6 Super Mini.

## Pinout

```
        [USB-C]
   LEFT           RIGHT
    TX  [1]   [1]  5V
    RX  [2]   [2]  GND
    IO0 [3]   [3]  3V3
    IO1 [4]   [4]  IO20
    IO2 [5]   [5]  IO19
    IO3 [6]   [6]  IO18
    IO4 [7]   [7]  IO15
    IO5 [8]   [8]  IO14
    IO6 [9]   [9]  IO9
    IO7 [10]  [10] IO8
```

## Hardware Setup

1. **Button**: Connect between IO0 and GND (directly, no resistor needed - uses internal pull-up)
2. **LED**: Connect one leg to GND, keep other leg free to plug into test pins

### LED Orientation
- **Longer leg (anode)**: Goes to the GPIO pin being tested (positive)
- **Shorter leg (cathode)**: Goes to GND

No external resistor needed - the GPIO drive strength and LED forward voltage provide sufficient current limiting for brief testing.

## Build & Upload

```bash
# Build only
pio run

# Build and upload (may fail on VMware, see below)
pio run -t upload

# Flash using script (more reliable with VMware)
./flash.sh /dev/ttyACM0

# Open serial monitor
pio device monitor
```

## Usage

1. Connect hardware (button on IO0, LED to GND)
2. Upload firmware and open serial monitor
3. Follow prompts:
   - **Phase 1**: Firmware automatically checks for shorts (to GND and between pins)
   - **Phase 2**: For each GPIO pin:
     - Plug LED's free leg into the pin
     - Press button to start blinking
     - Verify LED blinks
     - Press button to advance to next pin
   - **Final test**: Move button to IO1, connect LED to IO0, test IO0

## What It Tests

- **Phase 1 - Automatic Short Detection**
  - Shorts to GND (pins pulled LOW unexpectedly)
  - Shorts between adjacent pins

- **Phase 2 - Interactive GPIO Test**
  - Each pin toggles HIGH/LOW at 200ms intervals
  - Visual confirmation via LED blinking

## Success Criteria

- No shorts detected in Phase 1
- Board boots (onboard LED IO15 blinks 3x, serial output works)
- All 15 GPIO pins successfully blink the LED

## Flashing

### From Linux (with flash.sh)

```bash
./flash.sh /dev/ttyACM0
```

The script uses `--no-stub` flag which is more reliable for VMware USB passthrough.

### From Windows (esptool)

```bash
esptool.py --chip esp32c6 --port COM3 --no-stub write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Flash addresses:
- `bootloader.bin` → 0x0
- `partitions.bin` → 0x8000
- `firmware.bin` → 0x10000

## Lessons Learned

### ESP32-C6 Super Mini Specifics

1. **Flash Size**: The Super Mini has **4MB flash**, not 8MB like the official DevKit. Must configure in both:
   - `sdkconfig.defaults`: `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`
   - `platformio.ini`: `board_upload.flash_size = 4MB`

2. **Framework**: Arduino framework is NOT supported for ESP32-C6 with this board definition. Use ESP-IDF framework.

### ESP32-C6 USB Serial JTAG - Critical Information

The ESP32-C6 has a **built-in USB Serial/JTAG controller** - fundamentally different from older ESP32 boards that used external USB-UART bridges (CP2102, CH340).

#### Why USB Works in Download Mode But Not Normal Mode

- **Download mode** (hold BOOT, press RESET): The ROM bootloader *always* enables USB Serial/JTAG - hardcoded in ROM, independent of firmware.
- **Normal mode**: Your firmware must explicitly initialize USB. The `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` setting alone is NOT sufficient!

#### Required USB Initialization Code

Simply setting config options doesn't work. You MUST explicitly initialize in code:

```cpp
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"

void app_main(void) {
    // 1. Install USB Serial JTAG driver
    usb_serial_jtag_driver_config_t usb_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));

    // 2. Connect VFS (so printf works)
    esp_vfs_usb_serial_jtag_use_driver();

    // Now USB is active and printf() will work
}
```

#### Why printf() May Not Work

Even with the above, `printf()` may not output anything. For guaranteed output, use direct writes:

```cpp
static void usb_print(const char* msg) {
    usb_serial_jtag_write_bytes((const uint8_t*)msg, strlen(msg), pdMS_TO_TICKS(100));
}
```

#### sdkconfig.defaults Required Settings

```
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_SINGLE_APP=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y
```

The `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION` prevents the chip from entering light sleep when USB is connected, which would disconnect USB.

### PlatformIO + ESP-IDF

1. **Flash size mismatch errors**: If you see "Detected size(4096k) smaller than the size in the binary image header(8192k)", the bootloader has wrong flash size. Fix requires:
   - Setting `board_upload.flash_size = 4MB` in platformio.ini
   - Clean rebuild: `rm -rf .pio && pio run`
   - Full flash erase may be needed: `esptool.py --chip esp32c6 erase_flash`

2. **sdkconfig caching**: Changes to `sdkconfig.defaults` require deleting `.pio` folder to take effect.

3. **Bootloader rebuild**: If only `sdkconfig.defaults` changes, the bootloader may not rebuild. Always do `rm -rf .pio` after config changes.

### VMware USB Passthrough Issues

USB passthrough to Linux VMs can be unreliable for ESP32:

1. **Upload failures**: Use `--no-stub` flag with esptool for more reliable flashing
2. **Device disappears after reset**: VMware may not reconnect USB after device re-enumerates
3. **Serial data doesn't flow**: Device shows up but no data - use Windows host instead

#### VMware Workarounds

- Use `./flash.sh` which includes `--no-stub` flag
- Copy binaries to VMware shared folder, flash from Windows
- Add to .vmx file: `usb.autoConnect.device0 = "0x303a:0x1001"` for auto-connect

## Project Structure

```
├── platformio.ini          # PlatformIO configuration
├── sdkconfig.defaults      # ESP-IDF configuration defaults
├── flash.sh                # Reliable flash script with --no-stub
├── src/
│   └── main.cpp            # Main solder test firmware
├── examples/
│   └── minimal_usb_test.cpp  # Minimal USB Serial JTAG test
└── README.md               # This file
```

## References

- [ESP32-C6 Super Mini Pinout](https://www.espboards.dev/esp32/esp32-c6-super-mini/)
- [ESP-IDF USB Serial/JTAG Console](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/usb-serial-jtag-console.html)
- [PlatformIO ESP32 Platform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
