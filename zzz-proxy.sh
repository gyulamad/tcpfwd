#!/bin/bash

# Get the directory where THIS script is located (handles symlinks)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR" && ./builder $SCRIPT_DIR/cpptools/net/examples/zzz-proxy/zzz-proxy.cpp --mode=strict --run --run-args="9090 monster:8080 --startcmd=$SCRIPT_DIR/cpptools/net/examples/zzz-proxy/srv/start.sh >> $SCRIPT_DIR/cpptools/net/examples/zzz-proxy/srv/output.log 2>&1 --stopcmd=$SCRIPT_DIR/cpptools/net/examples/zzz-proxy/srv/stop.sh >> $SCRIPT_DIR/cpptools/net/examples/zzz-proxy/srv/output.log 2>&1 --timeout=180"