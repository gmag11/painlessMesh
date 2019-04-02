#!/bin/bash

inotify-hookable -w src -w test -c "rm bin/catch_*; cmake . -DCMAKE_CXX_FLAGS=\"-Wall -Werror\"; make; run-parts --regex catch_ bin/"
