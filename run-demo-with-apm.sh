#!/usr/bin/env bash
# Build and run the dice-server with the preload tracer, sending traces to the
# **local** APM Server (run Elasticsearch + APM Server first: see README).
# Requires: docker compose up -d in elasticsearch/ (which starts es01 + apm-server).

set -e
cd "$(dirname "$0")"

IMAGE_NAME="${IMAGE_NAME:-otel-cpp-demo}"
CONTAINER_NAME="${CONTAINER_NAME:-otel-cpp-demo-run}"

# Use host network bridge so we can reach apm-server by container name.
# Default: assume apm-server is on network elasticsearch_elastic (from elasticsearch/docker-compose).
APM_NETWORK="${APM_NETWORK:-elasticsearch_elastic}"

echo "Building Docker image ${IMAGE_NAME}..."
docker build -t "${IMAGE_NAME}" .

echo ""
echo "Starting dice-server with preload tracer → local APM Server..."
echo "  Server: http://localhost:8080 (try http://localhost:8080/rolldice)"
echo "  Traces: http://apm-server:8200 (OTLP) → Elasticsearch"
echo ""

docker run --rm -it \
  --name "${CONTAINER_NAME}" \
  -p 8080:8080 \
  --network "${APM_NETWORK}" \
  -e OTEL_SERVICE_NAME="${OTEL_SERVICE_NAME:-dice-server}" \
  -e OTEL_EXPORTER_OTLP_ENDPOINT="http://apm-server:8200/v1/traces" \
  -e OTEL_EXPORTER_OTLP_HEADERS="${OTEL_EXPORTER_OTLP_HEADERS:-}" \
  -e OTEL_ENV="${OTEL_ENV:-development}" \
  -e OTEL_SERVICE_VERSION="${OTEL_SERVICE_VERSION:-1.0}" \
  "${IMAGE_NAME}"
