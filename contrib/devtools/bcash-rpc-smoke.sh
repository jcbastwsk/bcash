#!/usr/bin/env bash

set -euo pipefail

show_help() {
  cat <<'EOF'
bcash-rpc-smoke.sh - quick non-destructive RPC connectivity checks

Usage:
  contrib/devtools/bcash-rpc-smoke.sh [OPTIONS]

Options:
  --cli PATH           Path to bitcoin-cli binary (default: ./src/bitcoin-cli)
  --rpcconnect HOST    RPC host (default: 127.0.0.1)
  --rpcport PORT       RPC port (optional)
  --rpcuser USER       RPC user (optional)
  --rpcpassword PASS   RPC password (optional)
  --help               Show this help and exit

Examples:
  # Use cookie auth from default data directory
  contrib/devtools/bcash-rpc-smoke.sh

  # Explicit credentials and RPC port
  contrib/devtools/bcash-rpc-smoke.sh --rpcport 9332 --rpcuser bcash --rpcpassword secret

Notes:
  - This script only runs read-only RPC methods.
  - It does not write chain data, restart daemons, or modify config.
EOF
}

CLI_BIN="./src/bitcoin-cli"
RPCCONNECT="127.0.0.1"
RPCPORT=""
RPCUSER=""
RPCPASSWORD=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cli)
      CLI_BIN="$2"
      shift 2
      ;;
    --rpcconnect)
      RPCCONNECT="$2"
      shift 2
      ;;
    --rpcport)
      RPCPORT="$2"
      shift 2
      ;;
    --rpcuser)
      RPCUSER="$2"
      shift 2
      ;;
    --rpcpassword)
      RPCPASSWORD="$2"
      shift 2
      ;;
    --help|-h)
      show_help
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      show_help
      exit 1
      ;;
  esac
done

if [[ ! -x "$CLI_BIN" ]]; then
  echo "ERROR: bitcoin-cli binary not found or not executable: $CLI_BIN" >&2
  echo "Hint: build first, or pass --cli /path/to/bitcoin-cli" >&2
  exit 1
fi

CLI_ARGS=("$CLI_BIN" "-rpcconnect=$RPCCONNECT")

if [[ -n "$RPCPORT" ]]; then
  CLI_ARGS+=("-rpcport=$RPCPORT")
fi

if [[ -n "$RPCUSER" ]]; then
  CLI_ARGS+=("-rpcuser=$RPCUSER")
fi

if [[ -n "$RPCPASSWORD" ]]; then
  CLI_ARGS+=("-rpcpassword=$RPCPASSWORD")
fi

run_rpc() {
  local method="$1"

  echo "==> $method"
  "${CLI_ARGS[@]}" "$method"
}

# Non-destructive read-only checks
run_rpc getblockchaininfo
run_rpc getnetworkinfo
run_rpc getmininginfo

echo
echo "RPC smoke check succeeded."
