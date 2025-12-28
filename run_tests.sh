#!/bin/bash
# Wrapper to run unit_tests with correct libstdc++ version
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libstdc++.so.6
exec ./build/unit_tests "$@"
