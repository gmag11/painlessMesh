#!/bin/bash


inotify-hookable -w src -c "platformio ci --lib="." --board=nodemcuv2 examples/basic/basic.ino"
