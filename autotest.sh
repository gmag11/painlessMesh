#!/bin/bash

inotify-hookable -w src -w test/startHere/startHere.ino -f CMakeLists.txt -c "platformio ci --lib=\".\" --board=esp32dev test/startHere/startHere.ino -O \"build_flags = -std=c++14\"" &

inotify-hookable -w src -w test -f CMakeLists.txt -c "rm bin/catch_*; cmake . -DCMAKE_CXX_FLAGS=\"-Wall -Werror\"; make -j4; run-parts --regex catch_ bin/"
