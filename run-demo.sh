#!/usr/bin/env bash
# Build and run the otel-cpp-demo (dice-server + preload tracer) in Docker.
# Optionally send traces to Elastic APM by setting env vars.

set -e
cd "$(dirname "$0")"

IMAGE_NAME="${IMAGE_NAME:-otel-cpp-demo}"
CONTAINER_NAME="${CONTAINER_NAME:-otel-cpp-demo-run}"

echo "Building Docker image ${IMAGE_NAME} (this may take several minutes)..."
docker build -t "${IMAGE_NAME}" .

echo ""
echo "Starting dice-server with preload tracer..."
echo "  Server will listen on http://localhost:8080"
echo "  Try: curl http://localhost:8080/rolldice"
echo ""

# Default: no endpoint (traces will be dropped but server runs).
# To send to Elastic APM, set before running:
#   export OTEL_EXPORTER_OTLP_ENDPOINT="https://<deployment>.apm.<region>.elastic-cloud.com/v1/traces"
#   export OTEL_EXPORTER_OTLP_HEADERS="Authorization=Bearer <secret token>"
docker run --rm -it \
  --name "${CONTAINER_NAME}" \
  -p 8080:8080 \
  -e OTEL_SERVICE_NAME="${OTEL_SERVICE_NAME:-dice-server}" \
  -e OTEL_EXPORTER_OTLP_ENDPOINT="${OTEL_EXPORTER_OTLP_ENDPOINT:-}" \
  -e OTEL_EXPORTER_OTLP_HEADERS="${OTEL_EXPORTER_OTLP_HEADERS:-}" \
  -e OTEL_ENV="${OTEL_ENV:-}" \
  -e OTEL_SERVICE_VERSION="${OTEL_SERVICE_VERSION:-}" \
  "${IMAGE_NAME}"
