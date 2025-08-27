# ==========================
# Stage 1: Build OpenTelemetry SDK and preload library
# ==========================
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential g++ cmake git wget curl \
    libgrpc++-dev libprotobuf-dev protobuf-compiler \
    libssl-dev sudo pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Clone OpenTelemetry C++ SDK
RUN git clone --branch main https://github.com/open-telemetry/opentelemetry-cpp.git otel-cpp

# Build and install OpenTelemetry SDK
WORKDIR /app/otel-cpp
RUN mkdir build
WORKDIR /app/otel-cpp/build
RUN cmake .. \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_OTLP_GRPC=ON \
    -DOPENTELEMETRY_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=/otel-cpp/install \
    -DWITH_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF && \
    cmake --build . --target all && \
    cmake --install .

# Build your preload library using CMake
WORKDIR /app
COPY libotel_preload.cpp .
RUN mkdir preload_build && cd preload_build && \
    cmake .. \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_PREFIX_PATH=/otel-cpp/install \
        -DBUILD_SHARED_LIBS=ON && \
    cmake --build . --target all

# ==========================
# Stage 2: Runtime image
# ==========================
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libgrpc++-dev libprotobuf-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy libraries from builder
COPY --from=builder /otel-cpp/install /otel-cpp/install
#COPY --from=builder /app/preload_build/libotel_preload.so /app/libotel_preload.so
#
## Set library path
#ENV LD_LIBRARY_PATH=/otel-cpp/install/lib:$LD_LIBRARY_PATH
#
#WORKDIR /app

# ==========================
# Stage 2: Runtime image
# ==========================
#FROM ubuntu:22.04

# Install minimal runtime dependencies
#RUN apt-get update && apt-get install -y \
#    libgrpc++1 libprotobuf23 libssl3 g++ build-essential iputils-ping \
#    && rm -rf /var/lib/apt/lists/*
#
#WORKDIR /app

# Copy the preload library and OpenTelemetry shared libraries from builder
#COPY --from=builder /app/libotel_preload.so .
#COPY --from=builder /otel-cpp/install /otel-cpp/install

# Copy server source and compile
#COPY ads_server.cpp .
#RUN g++ -std=c++17 -pthread ads_server.cpp -o ads_server \
#    -I/otel-cpp/install/include \
#    -L/otel-cpp/install/lib \
#    -lopentelemetry_trace \
#    -lopentelemetry_exporter_otlp_grpc \
#    -lopentelemetry_resources \
#    -lopentelemetry_common \
#    -lopentelemetry_sdk \
#    -lpthread -ldl -lgrpc++ -lprotobuf -lssl -lcrypto -lz

# Set environment for runtime
#ENV OTEL_EXPORTER_OTLP_ENDPOINT=http://otel_collector:4317
#ENV LD_LIBRARY_PATH=/otel-cpp/install/lib:$LD_LIBRARY_PATH
#ENV LD_PRELOAD=/app/libotel_preload.so

# Expose port and run
#EXPOSE 5000
#CMD ["./ads_server"]
