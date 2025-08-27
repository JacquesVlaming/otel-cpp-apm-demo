# Use the official OpenTelemetry Collector image as base
FROM otel/opentelemetry-collector:latest

# Copy your config file into the image
COPY otel-collector-config.yaml /etc/otel-collector-config.yaml

# Expose the OTLP ports
EXPOSE 4317 4318

# Set the default command to start the collector with your config
CMD ["--config=/etc/otel-collector-config.yaml"]
