#!/usr/bin/env bash
# Start Elasticsearch 9.2.5, wait for it to be ready, then generate and save
# the elastic password, Kibana enrollment token, http_ca.crt, and an API key.
# Usage: ./setup-tokens.sh

set -e
cd "$(dirname "$0")"

CREDENTIALS_FILE="${CREDENTIALS_FILE:-./credentials.txt}"
CA_CERT_FILE="${CA_CERT_FILE:-./http_ca.crt}"

# 1. Ensure ELASTIC_PASSWORD is set (used by compose and scripts)
if [[ -z "${ELASTIC_PASSWORD}" ]]; then
  if [[ -f .env ]] && grep -q '^ELASTIC_PASSWORD=' .env; then
    export ELASTIC_PASSWORD=$(grep '^ELASTIC_PASSWORD=' .env | cut -d= -f2-)
  else
    export ELASTIC_PASSWORD=$(openssl rand -base64 24 | tr -d '\n')
    echo "ELASTIC_PASSWORD=${ELASTIC_PASSWORD}" > .env
    echo "Wrote ELASTIC_PASSWORD to .env"
  fi
fi

# 2. Create Docker network if missing
docker network create elastic 2>/dev/null || true

# 3. Start Elasticsearch
echo "Starting Elasticsearch 9.2.5..."
docker compose up -d

# 4. Wait for Elasticsearch to be healthy
echo "Waiting for Elasticsearch to be ready (up to 5 minutes on first start)..."
for i in {1..60}; do
  if docker exec es01 curl -s -k -u "elastic:${ELASTIC_PASSWORD}" https://localhost:9200/_cluster/health 2>/dev/null | grep -q '"status":"green"\|"status":"yellow"'; then
    echo "Elasticsearch is ready."
    break
  fi
  if [[ $i -eq 60 ]]; then
    echo "Timeout waiting for Elasticsearch. Check: docker compose logs -f elasticsearch"
    exit 1
  fi
  sleep 5
done

# 5. Copy CA certificate
docker cp es01:/usr/share/elasticsearch/config/certs/http_ca.crt "${CA_CERT_FILE}" 2>/dev/null || true
if [[ -f "${CA_CERT_FILE}" ]]; then
  echo "Copied http_ca.crt to ${CA_CERT_FILE}"
fi

# 6 & 7. Generate Kibana token and API key inside the container (avoids host TLS/curl issues)
GEN_OUT=$(docker exec -e ELASTIC_PASSWORD="${ELASTIC_PASSWORD}" es01 /bin/bash -s < generate-tokens-inside.sh 2>/dev/null || true)

KIBANA_TOKEN=$(echo "$GEN_OUT" | sed -n '/KIBANA_TOKEN_START/,/KIBANA_TOKEN_END/p' | grep -v 'KIBANA_TOKEN_START\|KIBANA_TOKEN_END\|KIBANA_TOKEN_FAILED' | tr -d '\r')
if [[ -z "${KIBANA_TOKEN}" ]] || [[ "${KIBANA_TOKEN}" == *"FAILED"* ]]; then
  KIBANA_TOKEN="(run: docker exec -e ELASTIC_PASSWORD=\$(grep ELASTIC_PASSWORD .env | cut -d= -f2-) es01 /usr/share/elasticsearch/bin/elasticsearch-create-enrollment-token -s kibana)"
fi

API_KEY_JSON=$(echo "$GEN_OUT" | sed -n '/API_KEY_START/,/API_KEY_END/p' | grep -v 'API_KEY_START\|API_KEY_END' | tr -d '\n')
API_KEY_ID=$(echo "$API_KEY_JSON" | grep -o '"id":"[^"]*"' | cut -d'"' -f4)
API_KEY_VALUE=$(echo "$API_KEY_JSON" | grep -o '"encoded":"[^"]*"' | cut -d'"' -f4)
if [[ -z "${API_KEY_VALUE}" ]]; then
  API_KEY_VALUE="(create via: curl -sk -X POST -u elastic:\$ELASTIC_PASSWORD -H 'Content-Type: application/json' https://localhost:9200/_security/api_key -d '{\"name\":\"my-key\",\"expiration\":\"30d\"}' inside container)"
  API_KEY_ID=""
fi

# 8. Write credentials file
{
  echo "=== Elasticsearch 9.2.5 credentials ==="
  echo ""
  echo "Elastic user password:"
  echo "  ELASTIC_PASSWORD=${ELASTIC_PASSWORD}"
  echo ""
  echo "Elasticsearch URL (HTTPS):"
  echo "  https://localhost:9200"
  echo ""
  echo "CA certificate (for curl/scripts):"
  echo "  ${CA_CERT_FILE}"
  echo "  Example: curl --cacert ${CA_CERT_FILE} -u elastic:\$ELASTIC_PASSWORD https://localhost:9200"
  echo ""
  echo "Kibana enrollment token (valid 30 minutes):"
  echo "  ${KIBANA_TOKEN}"
  echo "  To regenerate: docker exec es01 /usr/share/elasticsearch/bin/elasticsearch-create-enrollment-token -s kibana"
  echo ""
  echo "API key (id: ${API_KEY_ID}):"
  echo "  ${API_KEY_VALUE}"
  echo "  Use as: Authorization: ApiKey <base64(id:value)> or Bearer <encoded> depending on client."
  echo ""
  echo "Reset elastic password:"
  echo "  docker exec -it es01 /usr/share/elasticsearch/bin/elasticsearch-reset-password -u elastic"
} > "${CREDENTIALS_FILE}"

echo ""
echo "Credentials and tokens written to: ${CREDENTIALS_FILE}"
echo ""
echo "Quick verification:"
echo "  curl -k -u elastic:\$ELASTIC_PASSWORD https://localhost:9200"
echo ""
echo "To run Kibana and use the enrollment token:"
echo "  docker run --name kib01 --net elastic -p 5601:5601 docker.elastic.co/kibana/kibana:9.2.5"
echo "  Then open http://localhost:5601 and enter the Kibana enrollment token from ${CREDENTIALS_FILE}"
