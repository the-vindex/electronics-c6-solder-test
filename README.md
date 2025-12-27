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

# Build and upload
pio run -t upload

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

## Flashing from Windows (esptool)

If flashing from Linux VM has USB issues, use Windows with esptool:

```bash
esptool.py --chip esp32c6 --port COM3 write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
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

2. **Console Output**: Uses USB Serial JTAG, not UART. Requires:
   - `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in sdkconfig.defaults

3. **Framework**: Arduino framework is NOT supported for ESP32-C6 with this board definition. Use ESP-IDF framework.

### PlatformIO + ESP-IDF

1. **Flash size mismatch errors**: If you see "Detected size(4096k) smaller than the size in the binary image header(8192k)", the bootloader has wrong flash size. Fix requires:
   - Setting `board_upload.flash_size = 4MB` in platformio.ini
   - Clean rebuild: `rm -rf .pio && pio run`
   - Full flash erase may be needed: `esptool.py --chip esp32c6 erase_flash`

2. **sdkconfig caching**: Changes to `sdkconfig.defaults` require deleting `.pio` folder to take effect.

### VMware USB Passthrough

USB passthrough to Linux VMs can be unreliable for ESP32 flashing. Workaround:
- Use VMware shared folders to copy firmware binaries
- Flash from Windows host using esptool

## Project Structure

```
├── platformio.ini      # PlatformIO configuration
├── sdkconfig.defaults  # ESP-IDF configuration defaults
├── src/
│   └── main.cpp        # Main firmware code
└── README.md           # This file
```

## References

- [ESP32-C6 Super Mini Pinout](https://www.espboards.dev/esp32/esp32-c6-super-mini/)
- [PlatformIO ESP32 Platform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
