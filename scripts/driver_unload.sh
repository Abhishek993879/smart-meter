#!/usr/bin/env bash
# Unload the pulsecnt driver.
set -euo pipefail
sudo rmmod pulsecnt
sudo dmesg | tail -2
