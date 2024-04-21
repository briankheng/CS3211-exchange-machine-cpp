#!/usr/bin/env bash

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <engine_path> <testcases_dir>"
  echo "The <testcases_dir> should contain .in files for testing."
  echo
  echo "Test results will be output in CSV format:"
  echo "testcase_name,exit_status"
  exit 1
fi

engine_path="$1"
testcases_dir="$2"

if [ ! -d "$testcases_dir" ]; then
  echo "Invalid test cases directory. Please provide a directory with .in files."
  exit 1
fi

for testcase in "$testcases_dir"/*.in; do
  testcase_name="$(basename "$testcase")"
  timeout 60s ./grader "$engine_path" < "$testcase" > /dev/null 2>&1
  echo "$testcase_name,$?"
done