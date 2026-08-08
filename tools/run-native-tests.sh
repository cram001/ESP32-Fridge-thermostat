#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "$project_dir/.native-test.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

compile_and_run() {
  source_file="$1"
  binary_name="$2"
  test_binary="$test_dir/$binary_name"

  c++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/test/native" \
    -I"$project_dir/include" \
    "$project_dir/test/native/$source_file" \
    -o "$test_binary"

  "$test_binary"
}

compile_and_run controller_tests.cpp controller-tests
compile_and_run failure_mode_tests.cpp failure-mode-tests
compile_and_run sensor_health_tests.cpp sensor-health-tests
compile_and_run cerbo_mqtt_interval_tests.cpp cerbo-mqtt-interval-tests
