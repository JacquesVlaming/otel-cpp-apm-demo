# Elasticsearch 9.2.5 on Docker

Single-node Elasticsearch 9.2.5 with security enabled (HTTPS, password, tokens).

## Prerequisites

- Docker and Docker Compose
- On Linux: set `vm.max_map_count` (e.g. `sudo sysctl -w vm.max_map_count=262144`) or the container may not start
- For HTTPS from the host: a curl/client that supports TLS 1.2+ (macOS LibreSSL may fail; use the container or another client)

## Quick start

```bash
./setup-tokens.sh
```

This will:

1. Generate or load `ELASTIC_PASSWORD` (stored in `.env`)
2. Start Elasticsearch in the background
3. Wait until the cluster is ready
4. Copy `http_ca.crt` for HTTPS verification
5. Generate a **Kibana enrollment token** (valid 30 minutes)
6. Create an **API key** (name: `otel-demo-api-key`, expiration: 30 days)
7. Write all credentials and tokens to `credentials.txt`

## APM Server (OTLP traces)

The compose file includes **APM Server** (port 8200). It receives OTLP traces (e.g. from the [otel-cpp-demo](../README.md) dice server) and writes them to Elasticsearch. From the repo root, run `./run-demo-with-apm.sh` so the dice server sends traces to `http://apm-server:8200/v1/traces`; then open **Kibana → Observability → APM → Services** and select **dice-server**.

## Manual steps

- **Start only:** `docker compose up -d`
- **Stop:** `docker compose down`
- **Regenerate Kibana token:**  
  `docker exec es01 /usr/share/elasticsearch/bin/elasticsearch-create-enrollment-token -s kibana`
- **Reset elastic password:**  
  `docker exec -it es01 /usr/share/elasticsearch/bin/elasticsearch-reset-password -u elastic`

## Verify

```bash
source .env
curl -k -u "elastic:${ELASTIC_PASSWORD}" https://localhost:9200
```

Or with the CA cert (after `setup-tokens.sh` has run):

```bash
curl --cacert http_ca.crt -u "elastic:${ELASTIC_PASSWORD}" https://localhost:9200
```

## Run Kibana (optional)

```bash
docker run --name kib01 --net elastic -p 5601:5601 docker.elastic.co/kibana/kibana:9.2.5
```

Open http://localhost:5601 and enter the Kibana enrollment token from `credentials.txt`. Log in as user `elastic` with `ELASTIC_PASSWORD` from `credentials.txt`.

## Optional: bootstrap with official one-shot run

If the standard `setup-tokens.sh` fails (e.g. Kibana token "HTTP layer SSL not configured with a keystore", or API key creation fails from the host due to TLS), use the **bootstrap** flow. It runs the official Elasticsearch image once with minimal config so the node creates the HTTP keystore and certs, generates tokens **inside** the container, and writes `credentials.txt`. The same volume `esdata` is used so you can then start the main compose and keep the cluster.

```bash
./bootstrap-official-es.sh
```

When it finishes, switch to the main compose (same data):

```bash
docker compose -f docker-compose.bootstrap.yml down
docker compose up -d
```

- **In-container token script:** `generate-tokens-inside.sh` is intended to be run inside the ES container (e.g. `docker exec -e ELASTIC_PASSWORD=... es01 /bin/bash -s < generate-tokens-inside.sh`). Both `setup-tokens.sh` and `bootstrap-official-es.sh` use it so the Kibana enrollment token and API key are created inside the container, avoiding host curl/TLS issues.
