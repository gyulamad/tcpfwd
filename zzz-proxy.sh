#!/bin/bash

# Get the directory where THIS script is located (handles symlinks)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR" && ./builder cpptools/net/examples/zzz-proxy/zzz-proxy.cpp --mode=strict --run --run-args="9090 monster:8080 --wakecmd=cpptools/net/examples/zzz-proxy/srv/ai/start.sh --checkcmd=cpptools/net/examples/zzz-proxy/srv/ai/is_loaded.sh --sleepcmd=cpptools/net/examples/zzz-proxy/srv/ai/shutdown.sh --timeout=120"