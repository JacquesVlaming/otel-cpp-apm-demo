# otel-cpp-demo

A C++ demo project showcasing OpenTelemetry instrumentation on Ubuntu.  
This project demonstrates how to collect and export traces to Elastic APM **without modifying the original application code** by using a **preload tracer library**.

The demo includes a simple `roll-dice` server application, and the `preload_tracer.so` library automatically instruments it at runtime. The preloader reports **APM-friendly context**: HTTP semantic attributes, span kind (server), response status codes, and proper span lifecycle (request starts on read, ends on write or close).

Based on https://opentelemetry.io/docs/languages/cpp/getting-started/

Once complete, your directory structure should resemble this:

$HOME/otel-cpp-demo  
├ images/  
├ oatpp/  
├ opentelemetry-cpp/  
├ roll-dice/   

## Prerequisites

Ensure that you have the following installed locally:

- Git
- C++ compiler supporting C++ version >= 14
- Make
- CMake version >= 3.25
- OpenSSL
- Zlib

---

## Steps

### Install Prerequisites:

```bash
sudo apt update
sudo apt install git
sudo apt install build-essential
sudo apt install cmake
sudo apt install libssl-dev
sudo apt install zlib1g-dev
```

---

### Clone the demo repository:

```bash
git clone https://github.com/elastic/otel-cpp-demo.git
cd otel-cpp-demo
```

---

### Quick test with Docker (Linux container)

To build and run the dice-server with the preload tracer **without** installing Oat++ or the OpenTelemetry SDK locally:

```bash
./run-demo.sh
```

Then in another terminal:

```bash
curl http://localhost:8080/rolldice
```

The server runs with tracing **disabled** until you set `OTEL_EXPORTER_OTLP_ENDPOINT` (and optionally `OTEL_EXPORTER_OTLP_HEADERS`).

**Option A – Local Elasticsearch + APM Server (traces in Kibana):**

1. Start Elasticsearch and APM Server: `cd elasticsearch && docker compose up -d` (then run `./setup-tokens.sh` if needed).
2. Run the dice server with tracing to local APM: `./run-demo-with-apm.sh`.
3. Generate traffic: `curl http://localhost:8080/rolldice`.
4. In Kibana go to **Observability → APM → Services** and open **dice-server**.

**Option B – Elastic Cloud APM:**

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT="https://<your-deployment>.apm.<region>.elastic-cloud.com/v1/traces"
export OTEL_EXPORTER_OTLP_HEADERS="Bearer <your-elastic-apm-secret-token>"
./run-demo.sh
```

---

### Build and install Oat++ framework

```bash
git clone https://github.com/oatpp/oatpp.git
cd oatpp
git checkout 1.3.0-latest
mkdir build
cd build

cmake ..
make
sudo make install
```

---

### Build and install OpenTelemetry C++ SDK

```bash
cd $HOME/otel-cpp-demo/
git clone https://github.com/open-telemetry/opentelemetry-cpp.git
cd opentelemetry-cpp
mkdir build
cd build

cmake -DBUILD_SHARED_LIBS=ON -DWITH_EXAMPLES=OFF -DWITH_OTLP_GRPC=ON -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF ..

cmake --build . -j$(nproc)
cmake --install . --prefix ../../otel-cpp
```

---

### Build the demo application

```bash
cd $HOME/otel-cpp-demo/roll-dice/
mkdir build
cd build
cmake ..
cmake --build .
```

---

### Build the preload tracer library

```bash
g++ -std=c++17 -shared -fPIC preload_tracer.cpp -o preload_tracer.so \
-I$HOME/otel-cpp-demo/otel-cpp/include \
$HOME/otel-cpp-demo/otel-cpp/lib/libopentelemetry_trace.so \
$HOME/otel-cpp-demo/otel-cpp/lib/libopentelemetry_exporter_otlp_http.so \
$HOME/otel-cpp-demo/otel-cpp/lib/libopentelemetry_resources.so \
$HOME/otel-cpp-demo/otel-cpp/lib/libopentelemetry_common.so
```

If you get undefined references to `context::` or propagation symbols, add your SDK’s context library, for example:  
`$HOME/otel-cpp-demo/otel-cpp/lib/libopentelemetry_context.so`

---

### Configure environment variables

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT="<your-elastic-apm-url>/v1/traces"
export OTEL_EXPORTER_OTLP_HEADERS="Bearer <your-elastic-apm-api-key>"
export OTEL_SERVICE_NAME="dice-server"
export LD_LIBRARY_PATH=$HOME/otel-cpp-demo/otel-cpp/lib:$LD_LIBRARY_PATH
```

Optional (for richer APM context):

- `OTEL_SERVER_ADDRESS` – server host (default: `localhost`)
- `OTEL_SERVER_PORT` – server port (default: `8080`)
- `OTEL_ENV` – deployment environment (e.g. `staging`, `production`)
- `OTEL_SERVICE_VERSION` – service version

> ⚠️ Replace `<your-elastic-apm-url>` with your actual Elastic APM URL.  
> ⚠️ Replace `<your-elastic-apm-api-key>` with your actual Elastic APM API key.

---

### Run the demo application with tracing

```bash
LD_PRELOAD=$HOME/otel-cpp-demo/roll-dice/preload_tracer.so ./build/dice-server
```
Open a new terminal and run curl commands against the dice-server to generate APM traces. The server exposes several routes so you can see different attributes in APM:

| Endpoint | Example | What shows in APM |
|----------|---------|-------------------|
| `GET /` | `curl http://localhost:8080/` | `url.path` = "/", small response |
| `GET /rolldice` | `curl http://localhost:8080/rolldice` | `url.path` = "/rolldice", status 200 |
| `GET /rolldice?sides=20` | `curl "http://localhost:8080/rolldice?sides=20"` | Query in path, status 200 |
| `GET /rolldice/slow?delay=2` | `curl "http://localhost:8080/rolldice/slow?delay=2"` | **Duration** (2s+), latency |
| `GET /rolldice/error` | `curl http://localhost:8080/rolldice/error` | **http.response.status_code** 500, span **status Error** |
| `GET /rolldice/big?sides=100` | `curl "http://localhost:8080/rolldice/big?sides=100"` | **http.response.body.size** (large payload) |
| `POST /rolldice` | `curl -X POST -d '{"sides":6}' http://localhost:8080/rolldice` | **http.request.method** POST, **http.request.body.size** |

Send a few requests (including `/rolldice/error` and `/rolldice/slow`) then in Kibana open **Observability → APM → Services → dice-server** and inspect transactions.

```bash
curl http://localhost:8080/rolldice
curl "http://localhost:8080/rolldice/slow?delay=2"
curl http://localhost:8080/rolldice/error
curl "http://localhost:8080/rolldice/big?sides=100"
curl -X POST -d '{"sides":6}' http://localhost:8080/rolldice
```

---

If everything is configured correctly, you’ll see the server start up and traces appear in your Elastic APM instance.

**What the preloader reports (APM context):** Span kind `server`; request attributes `http.request.method`, `url.path`, `http.request.body.size`; request headers `http.request.header.user_agent`, `http.request.header.x_request_id`; response attributes `http.response.status_code`, `http.response.body.size`; span status from HTTP status (OK vs Error for 4xx/5xx); `server.address` / `server.port`. **W3C Trace Context** is read from incoming `traceparent` / `tracestate` headers so this service attaches as a child in distributed traces. **Resource** includes `deployment.environment` and `service.version` when set via `OTEL_ENV` / `OTEL_SERVICE_VERSION`. Request span starts on first `read()` and ends on `write()` (response) or `close()`.

### How the preloader turns basic tracing into deeper insights

**Basic tracing** (e.g. a minimal OTLP span) typically gives you: a span name, start/end time (duration), and maybe a service name. You can see *that* a request happened and how long it took, but not *what* it was (path, method, status) or *why* it was slow or failed.

The **preloader adds HTTP semantics and lifecycle** so the same span becomes much more useful in APM:

| What you get | Why it matters |
|--------------|----------------|
| **`url.path`** | Filter and group by endpoint (e.g. `/rolldice/error` vs `/rolldice`), spot which routes are hit most or never. |
| **`http.request.method`** | Distinguish GET from POST/PUT; understand usage patterns and correlate with errors. |
| **`http.response.status_code`** | See 200 vs 4xx/5xx without opening logs; set **span status** to Error for 5xx so APM highlights failures. |
| **`http.request.body.size`** / **`http.response.body.size`** | Correlate payload size with latency or memory; spot oversized requests/responses. |
| **Request headers** (`user_agent`, `x_request_id`) | Debug client issues, trace a request across services using the same ID. |
| **W3C Trace Context** (incoming `traceparent`) | This service appears as a **child span** in a distributed trace when the caller sends trace context; you see the full path from gateway → service. |
| **Server address/port** | Know which instance or pod handled the request in multi-instance setups. |

The **dice-server routes** (`/`, `/rolldice`, `/rolldice/slow`, `/rolldice/error`, `/rolldice/big`, `POST /rolldice`) are designed to exercise these attributes: you’ll see different paths, status codes (200 vs 500), durations (slow path), and body sizes in Kibana. That’s how a single preload library turns “we have traces” into “we can see which endpoints fail, which are slow, and how they fit into a larger trace.”

![Demo Server Screenshot](images/dice-server.png)
