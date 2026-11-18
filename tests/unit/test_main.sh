#!/bin/sh
set -eu

out=$(mktemp)
trap 'rm -f "$out"' EXIT

for name in server client; do
    "$1/$name" > "$out"
    printf 'FileShare %s\n' "$name" | cmp - "$out"
    printf 'PASS: %s\n' "$name"
done

if [ -x "$1/test_db" ]; then
    "$1/test_db"
fi
