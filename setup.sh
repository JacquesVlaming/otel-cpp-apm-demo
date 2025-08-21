#!/bin/bash
set -e

# -------------------------------
# 1. Update system and install tools
# -------------------------------
sudo dnf update -y
sudo dnf install -y git cmake nano protobuf-devel grpc-devel abseil-cpp-devel
sudo dnf groupinstall -y "Development Tools"

# -------------------------------
# 2. Install Docker
# -------------------------------
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/rhel/docker-ce.repo
sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo systemctl enable --now docker

# -------------------------------
# 3. Run OpenTelemetry Collector in Docker (detached)
# -------------------------------
docker run -d --name otelcol \
  -v "$(pwd)/otel-collector-config.yaml:/etc/otelcol/config.yaml" \
  -p 4317:4317 -p 4318:4318 \
  otel/opentelemetry-collector:latest \
  --config /etc/otelcol/config.yaml

# -------------------------------
# 4. Clone repositories
# -------------------------------
git clone https://github.com/JacquesVlaming/otel-cpp-apm-demo.git
git clone https://github.com/open-telemetry/opentelemetry-cpp.git

# -------------------------------
# 5. Build and install OpenTelemetry C++ SDK
# -------------------------------
mkdir -p opentelemetry-cpp/build && cd opentelemetry-cpp/build
cmake .. \
  -DBUILD_SHARED_LIBS=ON \
  -DWITH_OTLP_GRPC=ON \
  -DCMAKE_INSTALL_PREFIX=$HOME/otel-cpp/install \
  -DWITH_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF
make -j$(nproc)
sudo make install
sudo ldconfig

# -------------------------------
# 6. Build the preload library
# -------------------------------
cd $HOME/otel-cpp-apm-demo
g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so \
  -I$HOME/otel-cpp/install/include \
  -L$HOME/otel-cpp/install/lib64 \
  -lopentelemetry_exporter_otlp_grpc \
  -lopentelemetry_trace \
  -lgrpc++ -lgrpc -ldl -lpthread \
  -Wl,-rpath,$HOME/otel-cpp/install/lib:$HOME/otel-cpp/install/lib64

# -------------------------------
# 7. Set environment variables
# -------------------------------
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317
export OTEL_EXPORTER_OTLP_INSECURE=true
export OTEL_RESOURCE_ATTRIBUTES=service.name=ads_server,env=dev
export LD_LIBRARY_PATH=$HOME/otel-cpp/install/lib:$LD_LIBRARY_PATH

# -------------------------------
# 8. Run the demo application with preload tracing
# -------------------------------
LD_PRELOAD=$HOME/otel-cpp-apm-demo/libotel_preload.so ./ads_server
