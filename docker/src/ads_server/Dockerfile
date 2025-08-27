# ==========================
# Stage 1: Build OpenTelemetry SDK and preload library
# ==========================
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential g++ cmake git wget curl \
    libgrpc++-dev libprotobuf-dev protobuf-compiler \
    libssl-dev sudo pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Clone OpenTelemetry C++ SDK
RUN git clone --branch main https://github.com/open-telemetry/opentelemetry-cpp.git /app/otel-cpp

# Build and install OpenTelemetry SDK
WORKDIR /app/otel-cpp/build
RUN cmake .. \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_OTLP_GRPC=ON \
    -DCMAKE_INSTALL_PREFIX=/otel-cpp/install \
    -DWITH_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
 && make -j$(nproc) \
 && make install \
 && ldconfig

# Copy preload source and build shared library
WORKDIR /app
COPY libotel_preload.cpp .
RUN g++ -std=c++17 -shared -fPIC libotel_preload.cpp -o libotel_preload.so \
    -I/otel-cpp/install/include \
    -L/otel-cpp/install/lib \
    -lopentelemetry_exporter_otlp_grpc \
    -lopentelemetry_trace \
    -lopentelemetry_common \
    -lgrpc++ -lgrpc -ldl -lpthread \
    -Wl,-rpath,/otel-cpp/install/lib

# Verify that preload library was built
RUN ls -l libotel_preload.so

# ==========================
# Stage 2: Runtime image
# ==========================
FROM ubuntu:22.04

# Install minimal runtime dependencies
RUN apt-get update && apt-get install -y \
    libgrpc++1 libprotobuf23 libssl3 g++ build-essential \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends iputils-ping \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the preload library and OpenTelemetry shared libraries from builder
COPY --from=builder /app/libotel_preload.so .
COPY --from=builder /otel-cpp/install /otel-cpp/install

# Copy your server source and compile
COPY ads_server.cpp .
RUN g++ -std=c++17 -pthread ads_server.cpp -o ads_server

# Set environment for runtime
ENV OTEL_EXPORTER_OTLP_ENDPOINT=http://otel_collector:4317
ENV LD_LIBRARY_PATH=/otel-cpp/install/lib:$LD_LIBRARY_PATH
ENV LD_PRELOAD=/app/libotel_preload.so


# Expose port and run
EXPOSE 5000
CMD ["./ads_server"]
