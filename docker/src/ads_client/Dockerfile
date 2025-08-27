# Use a lightweight image with build tools
FROM ubuntu:22.04

# Install build essentials and networking headers
RUN apt-get update && apt-get install -y \
    build-essential g++ iproute2 net-tools curl \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy the client source code
COPY ads_client.cpp .

# Compile the client
RUN g++ -std=c++17 -pthread ads_client.cpp -o ads_client

# Default command
CMD ["./ads_client"]
