#!/bin/bash
PORT=${1:-/dev/ttyACM0}
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32c6 --port "$PORT" run
