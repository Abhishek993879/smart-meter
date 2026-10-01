#!/usr/bin/env bash
# Build and load the pulsecnt driver. Extra arguments become module parameters,
# for example: scripts/driver_load.sh watts=2500 pulses_per_kwh=1000
set -euo pipefail
cd "$(dirname "$0")/../driver"
make
sudo rmmod pulsecnt 2>/dev/null || true
sudo insmod pulsecnt.ko "$@"
ls -l /dev/pulsecnt
sudo dmesg | tail -3
