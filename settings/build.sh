#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

CFLAGS=(-I../build/src/include)
LIBS=(../build/src/libkvdb.a -licuuc -licui18n -lpthread)

gcc settings.c "${CFLAGS[@]}" "${LIBS[@]}" -o settings

echo "built: settings"
