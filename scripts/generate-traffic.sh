#!/usr/bin/env bash
# Hit all dice-server endpoints to generate APM traces (different paths, status, duration, sizes).
# Usage: ./generate-traffic.sh [base_url]
# Default base_url: http://localhost:8080

set -e
BASE="${1:-http://localhost:8080}"

echo "Generating traffic to ${BASE} ..."

curl -s -o /dev/null -w "%{http_code} GET /\n" "${BASE}/"
curl -s -o /dev/null -w "%{http_code} GET /rolldice\n" "${BASE}/rolldice"
curl -s -o /dev/null -w "%{http_code} GET /rolldice/slow?delay=2\n" "${BASE}/rolldice/slow?delay=2"
curl -s -o /dev/null -w "%{http_code} GET /rolldice/error\n" "${BASE}/rolldice/error"
curl -s -o /dev/null -w "%{http_code} GET /rolldice/big?sides=100\n" "${BASE}/rolldice/big?sides=100"
curl -s -o /dev/null -w "%{http_code} POST /rolldice\n" -X POST -d '{"sides":6}' "${BASE}/rolldice"

echo "Done. Check Kibana → Observability → APM → dice-server"
