#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
CLI="${ARDUINO_CLI:-arduino-cli}"
"$CLI" core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
"$CLI" core install esp32:esp32@3.3.2 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
"$CLI" lib install ArduinoJson@7.4.2
python3 generate_web.py
"$CLI" compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=default_8MB,CDCOnBoot=cdc --output-dir bin firmware/Wisp
python3 package.py
