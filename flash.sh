#!/bin/bash
PORT=${1:-/dev/ttyACM0}
DIR="$(dirname "$0")/.pio/build/esp32-c6-devkitc-1"

python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32c6 \
    --port "$PORT" \
    --no-stub \
    write_flash \
    0x0 "$DIR/bootloader.bin" \
    0x8000 "$DIR/partitions.bin" \
    0x10000 "$DIR/firmware.bin"
