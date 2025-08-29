# OpenTelemetry C++ APM Demo Setup on Red Hat 9

This guide walks you through setting up a development environment for the **OpenTelemetry C++ APM demo** on **Red Hat 9**.  
It covers:

- Preparing the OpenTelemetry Collector configuration  
- Setting up Docker  
- Running the OpenTelemetry Collector  
- Running the demo application with tracing  

---

## **0. Prepare the OpenTelemetry Collector Configuration**

Before running the demo, ensure you have a **collector configuration file** named `otel-collector-config.yaml`.  
If this file does **not** exist in your working directory:

### **Step 0.1: Install Git**
```bash
sudo dnf update -y
sudo dnf install -y git
```

### **Step 0.2: Clone the Demo Repository**
```bash
git clone https://github.com/JacquesVlaming/otel-cpp-apm-demo.git
cd otel-cpp-apm-demo
```

### **Step 0.3: Copy Example Configuration**
```bash
cp otel-collector-config-example.yaml otel-collector-config.yaml
```

### **Step 0.4: Edit Configuration**
Open the file:
```bash
nano otel-collector-config.yaml
```

Update the following fields:

- **`exporters.otlp/elastic.endpoint`** → Your APM server URL  
  Example:
  ```
  https://<your-cluster-name>.apm.<region>.<provider>.elastic-cloud.com:443
  ```
- **`exporters.otlp/elastic.headers`** → Your API key in this format:
  ```
  Authorization: "ApiKey <your_api_key>"
  ```

---

## **1. Install Docker**

Docker is required to run the **OpenTelemetry Collector** in a container:

```bash
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/rhel/docker-ce.repo
sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
newgrp docker
```

---

## **2. Run the OpenTelemetry Collector**

Start the collector in **detached mode**:

```bash
docker run -d --name otelcol \
  -v "$(pwd)/otel-collector-config.yaml:/etc/otelcol/config.yaml" \
  -p 4317:4317 -p 4318:4318 \
  otel/opentelemetry-collector:latest \
  --config /etc/otelcol/config.yaml
```

This exposes the OTLP gRPC (`4317`) and HTTP (`4318`) ports for sending traces.

---

## **3. Set Environment Variables**

Configure the environment for tracing:

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317
export OTEL_EXPORTER_OTLP_INSECURE=true
export OTEL_RESOURCE_ATTRIBUTES=service.name=ads_server,env=dev
export LD_LIBRARY_PATH=$HOME/otel-cpp-apm-demo/otel-cpp/install/lib64:$LD_LIBRARY_PATH
export LD_PRELOAD=$HOME/otel-cpp-apm-demo/libotel_preload.so
```

---

## **4. Run the Demo Application with Preload Tracing**

Start the **ADS server**:

```bash
./ads_server
```

Expected output:
```
ADS Hello World Server listening on port 5000...
[OTEL PRELOAD] Tracing initialized (lazy)
```

---

## **5. Initiate Client Calls**

Open another terminal and run the client:

```bash
cd $HOME/otel-cpp-apm-demo
./ads_client
```

Expected output:
```
Server responded: Hello from ADS! You sent: Hello ADS Server!
```

This confirms that the OpenTelemetry library intercepted calls in `ads_server` and sent traces to the collector.

---

## ✅ **Summary Checklist**

✔ Prepare `otel-collector-config.yaml`  
✔ Install Docker & run the collector  
✔ Set environment variables  
✔ Run the demo with `LD_PRELOAD`  
✔ Send client requests  

After completing these steps, traces from the demo will be sent to the OpenTelemetry Collector and exported according to your configuration.
