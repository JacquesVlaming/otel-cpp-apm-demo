# This does not work yet!!

# Feel free to PR this... :)

# OTEL C++ APM Demo

This guide will show you how to set up the OpenTelemetry C++ demo on Debian / Red Hat 9 using Docker.

---

## Prerequisites

- Debian / Red Hat 9 with sudo privileges
- Internet access
- Sufficient disk space

---

## Step 1: Install Docker

### Debian

```sh
sudo apt-get update
sudo apt-get install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
$(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | \
sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

# RHEL 9

```sh
sudo yum update
sudo yum install -y ca-certificates curl
sudo yum install -y yum-utils device-mapper-persistent-data lvm2
sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
sudo yum install -y docker-ce docker-ce-cli containerd.io
sudo systemctl enable --now docker
```

```sh
# Add your user to the docker group to run docker commands without sudo
sudo usermod -aG docker $USER
newgrp docker
```

### Install Build Dependencies

#### Debian
```sh
sudo apt update
sudo apt install -y zlib1g-dev cmake g++ make protobuf-compiler libprotobuf-dev libgrpc-dev
```

#### RHEL 9

- Protocol Buffers (protobuf) is used to define the structure of telemetry data (like traces and metrics) that your application generates.
- gRPC is used to send this telemetry data from the demo application to the OpenTelemetry Collector (or other backend) over the network.

Specifically, when we build OpenTelemetry C++ with OTLP/gRPC support, the app serializes trace data using protobuf and transmits it using gRPC to the collector, enabling distributed tracing and observability.

TIP: Make use of `nohup` to run the `cmake` and `make` commands, and then look at the progress with `tail -f nohup.out`. For example:
```sh
nohup cmake .. -DCMAKE_BUILD_TYPE=Release &
tail -f nohup.out
```

```sh
# Build Protobuf
git clone https://github.com/protocolbuffers/protobuf.git
cd protobuf
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
sudo ldconfig
```

```sh
# Build gRPC
# Requires gRPC
git clone --recurse-submodules -b v1.62.0 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
cd cmake/build
cmake ../..
make -j$(nproc)
sudo make install
sudo ldconfig
```
---

## Step 2: Clone the Required Repositories

### Debian
```sh
# Install git
sudo apt update
sudo apt install -y git
```

### RHEL 9
```sh
# Install git
sudo yum update
sudo yum install -y git
```

```sh
git clone https://github.com/JacquesVlaming/otel-cpp-apm-demo.git
git clone https://github.com/open-telemetry/opentelemetry-cpp.git
```

---

## Step 3: Build the OpenTelemetry C++ SDK

### Debian
```sh
# Install required packages
sudo apt update
sudo apt install -y zlib1g-dev cmake g++ make
```

### RHEL 9
```sh
# Install required packages
sudo yum update
sudo yum install -y zlib-devel cmake gcc-c++ make
```

```sh
# Build the OpenTelemetry C++ SDK
# Requires gRPC
cd opentelemetry-cpp
mkdir build && cd build

# Configure the build
cmake .. \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_OTLP_GRPC=ON \
    -DCMAKE_INSTALL_PREFIX=$HOME/otel-cpp/install \
    -DWITH_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF

# Build the OpenTelemetry C++ SDK
make -j$(nproc)
# Install the SDK
sudo make install
# Update the shared library cache
sudo ldconfig
```

---

## Step 4: Build the Demo Applications

```sh
cd ~/otel-cpp-apm-demo
```


### Build Preload Library

```sh
g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so \
    -I$HOME/otel-cpp/install/include \
    -L$HOME/otel-cpp/install/lib \
    -lopentelemetry_exporter_otlp_grpc \
    -lopentelemetry_sdk_trace \
    -lopentelemetry_api \
    -ldl -lpthread
```

### Build Client and Server

```sh
g++ -std=c++17 -Wall -o ads_client ads_client.cpp
g++ -std=c++17 -Wall -pthread -o ads_server ads_server.cpp
```

---

## Step 5: Run the OpenTelemetry Collector

```sh
docker run --rm -it \
-v $(pwd)/otel-collector-config.yaml:/etc/otel-collector-config.yaml \
-p 4317:4317 -p 4318:4318 \
otel/opentelemetry-collector:latest \
--config=/etc/otel-collector-config.yaml
```

---

## Step 6: Run the Demo Server

```sh
export LD_LIBRARY_PATH=$HOME/otel-cpp/install/lib:$LD_LIBRARY_PATH
LD_PRELOAD=$HOME/otel-cpp-apm-demo/libotel_preload.so ./ads_server
```


### Run the Client to Generate Traces

```sh
./ads_client
```

---

## Notes

- Make sure `LD_LIBRARY_PATH` and `LD_PRELOAD` point to your compiled OpenTelemetry library.
- This setup allows you to observe traces sent from the demo application to the OpenTelemetry Collector running in Docker.
