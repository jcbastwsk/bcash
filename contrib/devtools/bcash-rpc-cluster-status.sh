#!/usr/bin/env bash
# Copyright (c) 2026 The BCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

set -euo pipefail

TIMEOUT_SECONDS="${BCASH_RPC_TIMEOUT_SECONDS:-3}"
STRICT_HEIGHTS=0

usage() {
  cat <<'EOF'
Usage:
  contrib/devtools/bcash-rpc-cluster-status.sh [--strict-heights] [--timeout SECONDS] name=url [name=url ...]

Examples:
  contrib/devtools/bcash-rpc-cluster-status.sh \
    node-0=http://user:pass@127.0.0.1:18443/ \
    node-1=http://user:pass@127.0.0.1:18444/

  BCASH_RPC_TIMEOUT_SECONDS=5 contrib/devtools/bcash-rpc-cluster-status.sh \
    --strict-heights \
    a=http://rpcuser:rpcpass@10.0.0.11:8332/ \
    b=http://rpcuser:rpcpass@10.0.0.12:8332/

Notes:
  - URLs can include rpcuser:rpcpassword credentials.
  - Uses JSON-RPC methods: getblockcount, getconnectioncount, getdifficulty.
  - Exit code is non-zero if any endpoint fails. With --strict-heights, it is
    also non-zero when node heights differ.
EOF
}

json_parse_result() {
  local json="$1"

  python3 - "$json" <<'PY'
import json
import sys

payload = json.loads(sys.argv[1])
if payload.get("error"):
    err = payload["error"]
    raise SystemExit(f"rpc_error:{err.get('code')}:{err.get('message')}")
value = payload.get("result")
if value is None:
    raise SystemExit("rpc_error:missing_result")
print(value)
PY
}

rpc_call() {
  local url="$1"
  local method="$2"

  curl --silent --show-error --max-time "${TIMEOUT_SECONDS}" \
    --header 'content-type: text/plain;' \
    --data "{\"jsonrpc\":\"1.0\",\"id\":\"bcash-cluster-status\",\"method\":\"${method}\",\"params\":[]}" \
    "${url}"
}

args=()
while (($#)); do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --strict-heights)
      STRICT_HEIGHTS=1
      shift
      ;;
    --timeout)
      if [ $# -lt 2 ]; then
        echo "error: --timeout requires a value" >&2
        exit 2
      fi
      TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --*)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      args+=("$1")
      shift
      ;;
  esac
done

if [ ${#args[@]} -eq 0 ]; then
  echo "error: provide at least one name=url endpoint" >&2
  usage >&2
  exit 2
fi

printf "%-14s %-8s %-8s %-14s %-s\n" "NODE" "HEIGHT" "PEERS" "DIFFICULTY" "STATUS"
printf "%-14s %-8s %-8s %-14s %-s\n" "--------------" "--------" "--------" "--------------" "------"

first_height=""
height_mismatch=0
failures=0

for pair in "${args[@]}"; do
  if [[ "${pair}" != *=* ]]; then
    echo "error: endpoint must be name=url, got: ${pair}" >&2
    exit 2
  fi

  name="${pair%%=*}"
  url="${pair#*=}"

  if [ -z "${name}" ] || [ -z "${url}" ]; then
    echo "error: invalid endpoint: ${pair}" >&2
    exit 2
  fi

  set +e
  height_json="$(rpc_call "${url}" getblockcount 2>&1)"
  height_rc=$?
  set -e

  if [ ${height_rc} -ne 0 ]; then
    printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "-" "-" "-" "ERROR: ${height_json}"
    failures=$((failures + 1))
    continue
  fi

  set +e
  height="$(json_parse_result "${height_json}" 2>&1)"
  parse_rc=$?
  set -e
  if [ ${parse_rc} -ne 0 ]; then
    printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "-" "-" "-" "ERROR: ${height}"
    failures=$((failures + 1))
    continue
  fi

  set +e
  peers_json="$(rpc_call "${url}" getconnectioncount 2>&1)"
  peers_rc=$?
  set -e
  if [ ${peers_rc} -ne 0 ]; then
    printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "${height}" "-" "-" "ERROR: ${peers_json}"
    failures=$((failures + 1))
    continue
  fi
  set +e
  peers="$(json_parse_result "${peers_json}" 2>&1)"
  peers_parse_rc=$?
  set -e
  if [ ${peers_parse_rc} -ne 0 ]; then
    printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "${height}" "-" "-" "ERROR: ${peers}"
    failures=$((failures + 1))
    continue
  fi

  set +e
  difficulty_json="$(rpc_call "${url}" getdifficulty 2>&1)"
  difficulty_rc=$?
  set -e
  if [ ${difficulty_rc} -ne 0 ]; then
    printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "${height}" "${peers}" "-" "ERROR: ${difficulty_json}"
    failures=$((failures + 1))
    continue
  fi
  set +e
  difficulty="$(json_parse_result "${difficulty_json}" 2>&1)"
  difficulty_parse_rc=$?
  set -e
  if [ ${difficulty_parse_rc} -ne 0 ]; then
    printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "${height}" "${peers}" "-" "ERROR: ${difficulty}"
    failures=$((failures + 1))
    continue
  fi

  status="OK"
  if [ -z "${first_height}" ]; then
    first_height="${height}"
  elif [ "${height}" != "${first_height}" ]; then
    status="WARN: height-mismatch"
    height_mismatch=1
  fi

  printf "%-14s %-8s %-8s %-14s %-s\n" "${name}" "${height}" "${peers}" "${difficulty}" "${status}"
done

if [ ${STRICT_HEIGHTS} -eq 1 ] && [ ${height_mismatch} -eq 1 ]; then
  failures=$((failures + 1))
fi

if [ ${failures} -gt 0 ]; then
  exit 1
fi
