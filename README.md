# OpenTelemetry C++ APM Demo Setup on Red Hat 9

This guide walks you through setting up a development environment for the OpenTelemetry C++ APM demo on Red Hat 9. It covers system updates, installing tools, Docker setup, building the OpenTelemetry C++ SDK, and running the demo application with tracing.

---

## 0. Prepare the OpenTelemetry Collector Configuration

Before running the demo, you need a configuration file named `otel-collector-config.yaml`. If this file does not exist in your working directory:

1. Clone the demo repository (if not already cloned):

Make sure you have at least git installed:
```bash
sudo dnf update -y
sudo dnf install -y git
```

Clone the repo:
```bash
git clone https://github.com/JacquesVlaming/otel-cpp-apm-demo.git
```

2. Copy the example configuration:

```bash
cp otel-cpp-apm-demo/otel-collector-config-example.yaml otel-collector-config.yaml
```

3. Edit `otel-collector-config.yaml` to match your environment:

```bash
nano otel-collector-config.yaml
```

This file configures the OpenTelemetry Collector to receive and export traces. Only update the `exporters.otlp/elastic.endpoint` and `exporters.otlp/elastic.headers`.

---

## 1. Update System and Install Tools

Update your system:

```bash
sudo dnf update -y
```

Install development tools and dependencies:

```bash
sudo dnf install -y git cmake nano protobuf-devel grpc-devel abseil-cpp-devel
sudo dnf groupinstall -y "Development Tools"
```

You might get errors, such as:
```bash
No match for argument: protobuf-devel
No match for argument: grpc-devel
No match for argument: abseil-cpp-devel
```

You can fix this, by first running this:
```bash
# 1. Install EPEL repository
sudo dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm

# 2. Enable CodeReady Builder repository
sudo dnf config-manager --set-enabled rhui-codeready-builder-for-rhel-9-x86_64-rhui-rpms

# 3. Update package metadata
sudo dnf update

# 4. Install required development packages
sudo dnf install -y git cmake nano protobuf-devel grpc-devel abseil-cpp-devel
```
These tools are required to build the OpenTelemetry C++ SDK and demo application.

---

## 2. Install Docker

Install Docker to run the OpenTelemetry Collector container:

```bash
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/rhel/docker-ce.repo
sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo systemctl enable --now docker
```

Docker allows running the collector in a containerized environment.

---

## 3. Run the OpenTelemetry Collector

Start the collector in detached mode:

```bash
docker run -d --name otelcol   -v "$(pwd)/otel-collector-config.yaml:/etc/otelcol/config.yaml"   -p 4317:4317 -p 4318:4318   otel/opentelemetry-collector:latest   --config /etc/otelcol/config.yaml
```

This exposes OTLP gRPC and HTTP ports for sending traces.

---

## 4. Clone Required Repositories

Clone the demo application and OpenTelemetry C++ SDK:

```bash
git clone https://github.com/JacquesVlaming/otel-cpp-apm-demo.git
git clone https://github.com/open-telemetry/opentelemetry-cpp.git
```

---

## 5. Build and Install OpenTelemetry C++ SDK

Navigate to the SDK source and create a build folder:

```bash
mkdir -p opentelemetry-cpp/build
cd opentelemetry-cpp/build
```

Configure and build:

```bash
cmake .. -DBUILD_SHARED_LIBS=ON -DWITH_OTLP_GRPC=ON -DCMAKE_INSTALL_PREFIX=$HOME/otel-cpp/install -DWITH_EXAMPLES=OFF -DBUILD_TESTING=OFF
make -j$(nproc)
sudo make install
sudo ldconfig
```

The SDK is installed into `$HOME/otel-cpp/install`.

---

## 6. Build the Preload Library

Navigate to the demo application directory:

```bash
cd $HOME/otel-cpp-apm-demo
```

Compile the preload library:

```bash
g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so   -I$HOME/otel-cpp/install/include   -L$HOME/otel-cpp/install/lib64   -lopentelemetry_exporter_otlp_grpc   -lopentelemetry_trace   -lgrpc++ -lgrpc -ldl -lpthread   -Wl,-rpath,$HOME/otel-cpp/install/lib:$HOME/otel-cpp/install/lib64
```

This library intercepts function calls to automatically generate traces for the demo app.

---

## 7. Set Environment Variables

Configure tracing:

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317
export OTEL_EXPORTER_OTLP_INSECURE=true
export OTEL_RESOURCE_ATTRIBUTES=service.name=ads_server,env=dev
export LD_LIBRARY_PATH=$HOME/otel-cpp/install/lib64:$LD_LIBRARY_PATH
```

---

## 8. Run the Demo Application with Preload Tracing

Start the demo application:

```bash
g++ -std=c++17 -pthread -o ads_server ads_server.cpp
LD_PRELOAD=$HOME/otel-cpp-apm-demo/libotel_preload.so ./ads_server
```

## 9. Initiate Client Calls from a Separate Terminal

Run the client to generate requests to `ads_server`:

```bash
g++ -std=c++17 -pthread -o ads_server ads_client.cpp
./ads_client
```

This ensures that the OpenTelemetry library intercepts calls in `ads_server` and sends traces to the collector.

---

## ✅ Summary

1. Ensure `otel-collector-config.yaml` exists (copy example if missing).  
2. Update system and install development tools.  
3. Install Docker and run the OpenTelemetry Collector.  
4. Clone demo and SDK repositories.  
5. Build and install OpenTelemetry C++ SDK.  
6. Build the preload library.  
7. Set environment variables for tracing.  
8. Run the demo application with `LD_PRELOAD`.  
9. Initiate client calls using `ads_client`.

After completing these steps, traces from the demo application will be collected and exported according to your OpenTelemetry Collector configuration.
