#!/usr/bin/env bash
# One-time bootstrap: run the official Elasticsearch 9.2.5 image so it
# creates the HTTP keystore and certs, then generate Kibana enrollment token
# and API key and save everything to credentials.txt. Uses the same volume
# "esdata" as the main docker-compose.yml so you can later run
# "docker compose up -d" and keep using the same cluster.
# Usage: ./bootstrap-official-es.sh

set -e
cd "$(dirname "$0")"

CREDENTIALS_FILE="${CREDENTIALS_FILE:-./credentials.txt}"
CA_CERT_FILE="${CA_CERT_FILE:-./http_ca.crt}"

# 1. Password
if [[ -z "${ELASTIC_PASSWORD}" ]]; then
  if [[ -f .env ]] && grep -q '^ELASTIC_PASSWORD=' .env; then
    export ELASTIC_PASSWORD=$(grep '^ELASTIC_PASSWORD=' .env | cut -d= -f2-)
  else
    export ELASTIC_PASSWORD=$(openssl rand -base64 24 | tr -d '\n')
    echo "ELASTIC_PASSWORD=${ELASTIC_PASSWORD}" > .env
    echo "Wrote ELASTIC_PASSWORD to .env"
  fi
fi

# 2. Network
docker network create elastic 2>/dev/null || true

# 3. Stop main compose if running so port 9200 is free
docker compose down 2>/dev/null || true

# 4. Run bootstrap compose (official image, minimal env, same volume)
echo "Starting Elasticsearch 9.2.5 (official bootstrap run)..."
docker compose -f docker-compose.bootstrap.yml up -d

# 5. Wait for green/yellow
echo "Waiting for Elasticsearch (up to 5 minutes)..."
for i in {1..60}; do
  if docker exec es01 curl -s -k -u "elastic:${ELASTIC_PASSWORD}" https://localhost:9200/_cluster/health 2>/dev/null | grep -qE '"status":"(green|yellow)"'; then
    echo "Elasticsearch is ready."
    break
  fi
  [[ $i -eq 60 ]] && { echo "Timeout."; docker compose -f docker-compose.bootstrap.yml logs; exit 1; }
  sleep 5
done

# 6. Copy CA cert
docker cp es01:/usr/share/elasticsearch/config/certs/http_ca.crt "${CA_CERT_FILE}" 2>/dev/null || true

# 7. Generate tokens inside container (so TLS and password are correct)
echo "Generating Kibana enrollment token and API key..."
GEN_OUT=$(docker exec -e ELASTIC_PASSWORD="${ELASTIC_PASSWORD}" es01 /bin/bash -s < generate-tokens-inside.sh 2>/dev/null || true)

KIBANA_TOKEN=$(echo "$GEN_OUT" | sed -n '/KIBANA_TOKEN_START/,/KIBANA_TOKEN_END/p' | grep -v 'KIBANA_TOKEN_START\|KIBANA_TOKEN_END\|KIBANA_TOKEN_FAILED' | tr -d '\r')
if [[ -z "${KIBANA_TOKEN}" ]] || [[ "${KIBANA_TOKEN}" == *"FAILED"* ]]; then
  KIBANA_TOKEN="(run: docker exec -e ELASTIC_PASSWORD=\$(grep ELASTIC_PASSWORD .env | cut -d= -f2-) es01 /usr/share/elasticsearch/bin/elasticsearch-create-enrollment-token -s kibana)"
fi

API_KEY_JSON=$(echo "$GEN_OUT" | sed -n '/API_KEY_START/,/API_KEY_END/p' | grep -v 'API_KEY_START\|API_KEY_END' | head -1)
API_KEY_ID=$(echo "$API_KEY_JSON" | grep -o '"id":"[^"]*"' | cut -d'"' -f4)
API_KEY_VALUE=$(echo "$API_KEY_JSON" | grep -o '"encoded":"[^"]*"' | cut -d'"' -f4)
if [[ -z "${API_KEY_VALUE}" ]]; then
  API_KEY_VALUE="(create via: curl -sk -X POST -u elastic:\$ELASTIC_PASSWORD -H 'Content-Type: application/json' https://localhost:9200/_security/api_key -d '{\"name\":\"my-key\",\"expiration\":\"30d\"}')"
  API_KEY_ID=""
fi

# 8. Write credentials
{
  echo "=== Elasticsearch 9.2.5 credentials ==="
  echo ""
  echo "Elastic user password:"
  echo "  ELASTIC_PASSWORD=${ELASTIC_PASSWORD}"
  echo ""
  echo "Elasticsearch URL (HTTPS):"
  echo "  https://localhost:9200"
  echo ""
  echo "CA certificate:"
  echo "  ${CA_CERT_FILE}"
  echo ""
  echo "Kibana enrollment token (valid 30 minutes):"
  echo "  ${KIBANA_TOKEN}"
  echo ""
  echo "API key (id: ${API_KEY_ID}):"
  echo "  ${API_KEY_VALUE}"
  echo ""
  echo "Reset elastic password:"
  echo "  docker exec -it es01 /usr/share/elasticsearch/bin/elasticsearch-reset-password -u elastic"
} > "${CREDENTIALS_FILE}"

echo ""
echo "Credentials written to ${CREDENTIALS_FILE}"
echo ""
echo "Bootstrap container is still running. To switch to the main compose (same data):"
echo "  docker compose -f docker-compose.bootstrap.yml down"
echo "  docker compose up -d"
echo ""
echo "To run Kibana:"
echo "  docker run --name kib01 --net elastic -p 5601:5601 docker.elastic.co/kibana/kibana:9.2.5"
echo "  Open http://localhost:5601 and enter the Kibana enrollment token from ${CREDENTIALS_FILE}"
