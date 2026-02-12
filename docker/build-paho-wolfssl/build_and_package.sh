#!/bin/bash
set -euo pipefail

echo "Starting build: wolfSSL already provided by libwolfssl-dev package"

# Work in /work; host repo should be mounted here when running container
OUTPUT_DIR=/work/output
mkdir -p "$OUTPUT_DIR"

echo "Clone and build paho.mqtt.c with OpenSSL TLS support"
if [ ! -d paho.mqtt.c ]; then
  git clone https://github.com/eclipse/paho.mqtt.c.git
fi
cd paho.mqtt.c
mkdir -p build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_BUILD_STATIC=OFF
make -j$(nproc)
make install || true
ldconfig || true

echo "Build publisher binary linking with paho and OpenSSL (with embedded library path)"
cd /work
if [ -f src/publisher/publisher.c ]; then
  # バイナリに /usr/local/lib のライブラリパスを埋め込む (-Wl,-rpath)
  gcc src/publisher/publisher.c -o "$OUTPUT_DIR/publisher_bin" \
    -lpaho-mqtt3cs -lssl -lcrypto -pthread \
    -Wl,-rpath,/usr/local/lib
  echo "Built publisher binary at $OUTPUT_DIR/publisher_bin"
else
  echo "publisher.c not found in /work/src/publisher; ensure repository mounted correctly"
  exit 1
fi

echo "Build finished"
