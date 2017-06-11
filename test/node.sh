#!/bin/bash

#inotify-hookable -w src -c "platformio ci --lib="." --board=nodemcuv2 examples/basic/basic.ino"
pio run -d test/basic/ -t upload; pio device monitor -b 115200

pio run -d test/time/ -t upload; pio device monitor -b 115200
