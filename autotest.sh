#!/bin/bash

inotify-hookable -w src -w test -c "rm bin/catch_*; cmake . -DCMAKE_CXX_FLAGS=\"-Wall -Werror\"; make -j4; run-parts --regex catch_ bin/"
