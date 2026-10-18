#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"
g++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/test/host/test_sally_policy.cpp" -o /tmp/test_sally_policy
/tmp/test_sally_policy
python3 "$ROOT/test/host/check_roots.py"
