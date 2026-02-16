#!/usr/bin/env bash
# Run this script INSIDE the Elasticsearch container (e.g. via docker exec)
# with ELASTIC_PASSWORD set. It prints Kibana enrollment token and API key
# in a parseable format so the host can save them to credentials.txt.
# Usage (from host): docker exec -e ELASTIC_PASSWORD="$(grep ELASTIC_PASSWORD .env | cut -d= -f2-)" es01 /bin/bash -s < generate-tokens-inside.sh

set -e
echo "KIBANA_TOKEN_START"
/usr/share/elasticsearch/bin/elasticsearch-create-enrollment-token -s kibana 2>/dev/null | tail -1 || echo "KIBANA_TOKEN_FAILED"
echo "KIBANA_TOKEN_END"

echo "API_KEY_START"
API_KEY_RESPONSE=$(curl -s -k -X POST -u "elastic:${ELASTIC_PASSWORD}" \
  -H "Content-Type: application/json" \
  "https://localhost:9200/_security/api_key" \
  -d '{"name":"otel-demo-api-key","expiration":"30d"}' 2>/dev/null || echo '{}')
echo "$API_KEY_RESPONSE" | tr -d '\n'
echo ""
echo "API_KEY_END"
