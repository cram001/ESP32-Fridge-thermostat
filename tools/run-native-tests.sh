#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "$project_dir/.native-test.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
test_binary="$test_dir/controller-tests"

c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$project_dir/test/native" \
  -I"$project_dir/include" \
  "$project_dir/test/native/controller_tests.cpp" \
  -o "$test_binary"

"$test_binary"
