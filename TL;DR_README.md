
# OpenTelemetry C++ APM Demo Setup on Red Hat

This guide walks you through setting up a development environment for the **OpenTelemetry C++ APM demo** on **Red Hat**.  
It covers:

- Preparing the OpenTelemetry Collector configuration  
- Setting environment variables  
- Installing and running Docker  
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

### **Step 0.5: Set Environment Variables**

Create a new file in `/etc/profile.d/`, e.g.,
```bash
sudo nano /etc/profile.d/otel.sh
```

Add your environment variables:
```bash
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317
export OTEL_EXPORTER_OTLP_INSECURE=true
export LD_LIBRARY_PATH=$HOME/otel-cpp-apm-demo/otel-cpp/install/lib64:$HOME/otel-cpp-apm-demo/otel-cpp/install/lib:$LD_LIBRARY_PATH
export LD_PRELOAD=$HOME/otel-cpp-apm-demo/libotel_preload.so
```

Make it executable:
```bash
sudo chmod +x /etc/profile.d/otel.sh
```

> **Tip:** Open a new terminal or `source /etc/profile.d/otel.sh` to load these variables immediately.  

#### **0.5.1 Reboot the System**
To ensure all environment variables are loaded system-wide:
```bash
sudo reboot
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
docker run -d --name otelcol   -v "$(pwd)/otel-collector-config.yaml:/etc/otelcol/config.yaml"   -p 4317:4317 -p 4318:4318   otel/opentelemetry-collector:latest   --config /etc/otelcol/config.yaml
```

This exposes the OTLP gRPC (`4317`) and HTTP (`4318`) ports for sending traces.

---

## **3. Run the Demo Application with Preload Tracing**

### **3.1 Start the Server**
```bash
cd $HOME/otel-cpp-apm-demo
export OTEL_RESOURCE_ATTRIBUTES=service.name=ads_server,env=dev
./ads_server
```

Expected output:
```
ADS Hello World Server listening on port 5000...
[OTEL PRELOAD] Tracing initialized (lazy)
```

### **3.2 Start the Client**

Open another terminal:
```bash
cd $HOME/otel-cpp-apm-demo
export OTEL_RESOURCE_ATTRIBUTES=service.name=ads_client,env=dev
./ads_client
```

Expected output:
```
[OTEL PRELOAD] Tracing initialized (lazy)
Server responded: Hello from ADS! You sent: Hello ADS Server!
```

This confirms that the OpenTelemetry library intercepted calls in `ads_server` and sent traces to the collector.

---

## ✅ **Summary Checklist**

✔ Prepare `otel-collector-config.yaml`  
✔ Set environment variables globally and reboot  
✔ Install Docker & run the collector  
✔ Run the demo with `LD_PRELOAD`  
✔ Send client requests and verify traces  

---

Traces from the demo are now sent to the OpenTelemetry Collector and exported according to your configuration.
