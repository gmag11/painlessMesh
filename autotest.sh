#!/bin/bash

export PLATFORMIO_BUILD_FLAGS="-D UNIT_TEST"
#inotify-hookable -w src -c "platformio ci --board=nodemcuv2 --lib="." test/test_main.cpp"
#inotify-hookable -w src -w test -c "platformio test"
inotify-hookable -w src -w test -c "rm bin/catch_*; cmake . -G Ninja -DCMAKE_CXX_FLAGS=\"-Wall -Werror\"; ninja && run-parts --regex catch_ bin/"
