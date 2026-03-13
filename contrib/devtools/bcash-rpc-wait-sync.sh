#!/usr/bin/env bash
# Copyright (c) 2026 The BCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

set -euo pipefail

INTERVAL_SECONDS="${BCASH_RPC_POLL_INTERVAL_SECONDS:-2}"
TIMEOUT_SECONDS="${BCASH_RPC_TIMEOUT_SECONDS:-120}"
RPC_TIMEOUT_SECONDS="${BCASH_RPC_SINGLE_CALL_TIMEOUT_SECONDS:-3}"
MIN_PEERS="${BCASH_RPC_MIN_PEERS:-0}"
QUIET=0

usage() {
  cat <<'EOF'
Usage:
  contrib/devtools/bcash-rpc-wait-sync.sh [options] name=url [name=url ...]

Options:
  --timeout SECONDS    Maximum total wait time (default: 120)
  --interval SECONDS   Poll interval between checks (default: 2)
  --rpc-timeout SEC    Per-RPC request timeout for curl (default: 3)
  --min-peers N        Require each node to have at least N peers (default: 0)
  --quiet              Print only final status line
  -h, --help           Show this help message

Environment overrides:
  BCASH_RPC_TIMEOUT_SECONDS
  BCASH_RPC_POLL_INTERVAL_SECONDS
  BCASH_RPC_SINGLE_CALL_TIMEOUT_SECONDS
  BCASH_RPC_MIN_PEERS

Examples:
  contrib/devtools/bcash-rpc-wait-sync.sh \
    node-0=http://user:pass@127.0.0.1:18443/ \
    node-1=http://user:pass@127.0.0.1:18444/

  contrib/devtools/bcash-rpc-wait-sync.sh --timeout 300 --min-peers 1 \
    node-0=http://rpcuser:rpcpass@10.0.0.11:8332/ \
    node-1=http://rpcuser:rpcpass@10.0.0.12:8332/ \
    node-2=http://rpcuser:rpcpass@10.0.0.13:8332/

Notes:
  - This script is non-destructive. It only calls read-only RPC methods:
    getblockcount and getconnectioncount.
  - Success means all endpoints are reachable, all heights are equal,
    and each node has at least --min-peers peers.
EOF
}

is_nonnegative_int() {
  [[ "$1" =~ ^[0-9]+$ ]]
}

rpc_call() {
  local url="$1"
  local method="$2"

  curl --silent --show-error --max-time "${RPC_TIMEOUT_SECONDS}" \
    --header 'content-type: text/plain;' \
    --data "{\"jsonrpc\":\"1.0\",\"id\":\"bcash-rpc-wait-sync\",\"method\":\"${method}\",\"params\":[]}" \
    "${url}"
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

args=()
while (($#)); do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --timeout)
      if [ $# -lt 2 ]; then
        echo "error: --timeout requires a value" >&2
        exit 2
      fi
      TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --interval)
      if [ $# -lt 2 ]; then
        echo "error: --interval requires a value" >&2
        exit 2
      fi
      INTERVAL_SECONDS="$2"
      shift 2
      ;;
    --rpc-timeout)
      if [ $# -lt 2 ]; then
        echo "error: --rpc-timeout requires a value" >&2
        exit 2
      fi
      RPC_TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --min-peers)
      if [ $# -lt 2 ]; then
        echo "error: --min-peers requires a value" >&2
        exit 2
      fi
      MIN_PEERS="$2"
      shift 2
      ;;
    --quiet)
      QUIET=1
      shift
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

for value in "${TIMEOUT_SECONDS}" "${INTERVAL_SECONDS}" "${RPC_TIMEOUT_SECONDS}" "${MIN_PEERS}"; do
  if ! is_nonnegative_int "${value}"; then
    echo "error: numeric options must be non-negative integers" >&2
    exit 2
  fi
done

start_ts="$(date +%s)"
attempt=0

while :; do
  attempt=$((attempt + 1))
  failures=0
  first_height=""
  height_mismatch=0
  peer_shortfall=0
  summary=()

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
      failures=$((failures + 1))
      summary+=("${name}:rpc-error")
      continue
    fi

    set +e
    height="$(json_parse_result "${height_json}" 2>&1)"
    parse_height_rc=$?
    set -e
    if [ ${parse_height_rc} -ne 0 ]; then
      failures=$((failures + 1))
      summary+=("${name}:height-parse-error")
      continue
    fi

    set +e
    peers_json="$(rpc_call "${url}" getconnectioncount 2>&1)"
    peers_rc=$?
    set -e
    if [ ${peers_rc} -ne 0 ]; then
      failures=$((failures + 1))
      summary+=("${name}:peer-rpc-error")
      continue
    fi

    set +e
    peers="$(json_parse_result "${peers_json}" 2>&1)"
    parse_peers_rc=$?
    set -e
    if [ ${parse_peers_rc} -ne 0 ]; then
      failures=$((failures + 1))
      summary+=("${name}:peer-parse-error")
      continue
    fi

    if [ -z "${first_height}" ]; then
      first_height="${height}"
    elif [ "${height}" != "${first_height}" ]; then
      height_mismatch=1
    fi

    if [ "${peers}" -lt "${MIN_PEERS}" ]; then
      peer_shortfall=1
      summary+=("${name}:height=${height},peers=${peers}")
    else
      summary+=("${name}:height=${height},peers=${peers}")
    fi
  done

  now_ts="$(date +%s)"
  elapsed=$((now_ts - start_ts))

  if [ ${failures} -eq 0 ] && [ ${height_mismatch} -eq 0 ] && [ ${peer_shortfall} -eq 0 ]; then
    [ ${QUIET} -eq 0 ] && echo "attempt=${attempt} elapsed=${elapsed}s status=READY details=[${summary[*]}]"
    echo "READY: all nodes reachable, heights aligned, peers >= ${MIN_PEERS}"
    exit 0
  fi

  if [ ${QUIET} -eq 0 ]; then
    state_parts=()
    [ ${failures} -gt 0 ] && state_parts+=("rpc_failures=${failures}")
    [ ${height_mismatch} -eq 1 ] && state_parts+=("height_mismatch=1")
    [ ${peer_shortfall} -eq 1 ] && state_parts+=("peer_shortfall=1")
    echo "attempt=${attempt} elapsed=${elapsed}s status=WAIT ${state_parts[*]} details=[${summary[*]}]"
  fi

  if [ "${elapsed}" -ge "${TIMEOUT_SECONDS}" ]; then
    echo "TIMEOUT: cluster did not reach sync criteria within ${TIMEOUT_SECONDS}s" >&2
    exit 1
  fi

  sleep "${INTERVAL_SECONDS}"
done
