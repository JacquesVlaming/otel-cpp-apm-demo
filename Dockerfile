# Build and run otel-cpp-demo (dice-server + preload tracer) on Linux
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    libssl-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Oat++ (build in-tree so roll-dice CMake finds build/src/liboatpp.a)
RUN git clone --depth 1 https://github.com/oatpp/oatpp.git -b 1.3.0-latest \
    && cd oatpp && mkdir build && cd build \
    && cmake .. && make -j$(nproc) \
    && cd /build

# OpenTelemetry C++ SDK (with OTLP HTTP for Elastic APM)
RUN git clone --depth 1 https://github.com/open-telemetry/opentelemetry-cpp.git \
    && cd opentelemetry-cpp && mkdir build && cd build \
    && cmake .. \
        -DCMAKE_INSTALL_PREFIX=/opt/otel-cpp \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_EXAMPLES=OFF \
        -DWITH_OTLP_GRPC=OFF \
        -DWITH_OTLP_HTTP=ON \
        -DBUILD_TESTING=OFF \
        -DWITH_BENCHMARK=OFF \
        -DWITH_FUNC_TESTS=OFF \
    && cmake --build . -j$(nproc) && cmake --install . \
    && cd /build && rm -rf opentelemetry-cpp

# Dice server app (OATPP_ROOT points to oatpp source with build/src/liboatpp.a)
COPY roll-dice /build/roll-dice
RUN cd /build/roll-dice && mkdir build && cd build \
    && cmake .. -DOATPP_ROOT=/build/oatpp \
    && cmake --build .

# Preload tracer library (needs trace, otlp http, resources, common; context may be in trace)
RUN cd /build/roll-dice \
    && g++ -std=c++17 -shared -fPIC preload_tracer.cpp -o preload_tracer.so \
        -I/opt/otel-cpp/include \
        -L/opt/otel-cpp/lib -Wl,-rpath,/opt/otel-cpp/lib \
        -lopentelemetry_trace \
        -lopentelemetry_exporter_otlp_http \
        -lopentelemetry_resources \
        -lopentelemetry_common \
        -ldl -lpthread

# Runtime image
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/otel-cpp /opt/otel-cpp
COPY --from=builder /build/roll-dice/build/dice-server /app/dice-server
COPY --from=builder /build/roll-dice/preload_tracer.so /app/preload_tracer.so

ENV LD_LIBRARY_PATH=/opt/otel-cpp/lib
ENV OTEL_SERVICE_NAME=dice-server
ENV OTEL_SERVER_ADDRESS=0.0.0.0
ENV OTEL_SERVER_PORT=8080

# Override at run: OTEL_EXPORTER_OTLP_ENDPOINT, OTEL_EXPORTER_OTLP_HEADERS
EXPOSE 8080
WORKDIR /app
CMD ["/bin/bash", "-c", "LD_PRELOAD=/app/preload_tracer.so /app/dice-server"]
