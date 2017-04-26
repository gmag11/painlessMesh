#!/bin/bash
echo ""
echo "Note that this test requires a running echoNode, which can be flashed with the following command:"
echo "pio run -d test/echoNode -t upload"
echo ""
echo ""

pio run -d test/network/ -t upload; pio device monitor -b 115200
