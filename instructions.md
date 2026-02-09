# Bcash - FreeBSD Install & Usage

Bcash v0.1.0 — a headless cryptocurrency node based on Bitcoin 0.01 by Satoshi Nakamoto.
Currently runs as a command-line daemon with a JSON-RPC interface. The original wxWidgets
GUI has not been ported to FreeBSD yet.

## Dependencies

Install with `pkg`:

```sh
pkg install boost-libs db5 openssl
```

These are the runtime and build dependencies:

| Package | Purpose |
|---------|---------|
| boost-libs | Boost C++ libraries (foreach, lexical_cast, tuple, array) |
| db5 | Berkeley DB 5.3 (wallet & block index storage) |
| openssl | ECDSA, SHA-256, RIPEMD-160 (built-in on FreeBSD 15, but dev headers needed) |

All three should already be present if you've built the binary before.

## Building

Bcash uses GNU make (not BSD make):

```sh
cd ~/bcash
gmake -f Makefile.bsd
```

This produces the `bcash` binary in the current directory. To rebuild from scratch:

```sh
gmake -f Makefile.bsd clean
gmake -f Makefile.bsd
```

The build uses the system C++ compiler (`c++` / clang) with `-std=c++11`.

## Running

### Start the node

```sh
./bcash
```

This will:
1. Load (or create) the wallet and block index in `~/.bcash/`
2. Start the P2P network node
3. Start the JSON-RPC server on `127.0.0.1:9332`
4. Start mining by default

### Command-line options

```
./bcash [options]

  -nogenerate     Don't mine blocks
  -solo           Mine without peers (solo/bootstrap mode)
  -datadir <dir>  Use a custom data directory
  -debug          Enable debug output
  -help           Show help
```

For first-time solo use (no peers on the network yet):

```sh
./bcash -solo
```

### Stop the node

Press `Ctrl+C`. The node will flush the database and shut down cleanly.

## JSON-RPC Interface

The RPC server listens on `127.0.0.1:9332` and accepts standard JSON-RPC
over HTTP POST. Only localhost connections are allowed.

### Example with curl

```sh
# Get node info
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getinfo","params":[],"id":1}'

# Get wallet balance
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getbalance","params":[],"id":1}'

# Get current block height
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getblockcount","params":[],"id":1}'

# Generate a new receiving address
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getnewaddress","params":[],"id":1}'

# Send coins
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"sendtoaddress","params":["<address>","<amount>"],"id":1}'
```

### Available RPC methods

| Method | Params | Description |
|--------|--------|-------------|
| `getinfo` | none | Version, balance, block height, connections, bgold height |
| `getbalance` | none | Wallet balance in BCASH |
| `getblockcount` | none | Current best block height |
| `getnewaddress` | none | Generate and return a new address |
| `sendtoaddress` | `[address, amount]` | Send BCASH to an address |
| `listproducts` | none | List marketplace products |
| `listnews` | none | List recent news items (top 20) |
| `submitnews` | `[title, url, text]` | Submit a news item |
| `votenews` | `[hash, upvote]` | Vote on a news item (upvote: `true`/`false`) |
| `getbgoldbalance` | none | Get bgold token balance |

### Pipe to jq for readable output

```sh
pkg install jq  # if not installed

curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getinfo","params":[],"id":1}' | jq .
```

## Data directory

Default: `~/.bcash/`

Contains:
- `blkindex.dat` — block index database (Berkeley DB)
- `blk0001.dat` — block data
- `wallet.dat` — private keys and transactions
- `addr.dat` — known peer addresses

Back up `wallet.dat` to protect your coins.

## GUI status

The original Bitcoin 0.01 GUI used wxWidgets 2.8 on Windows. The FreeBSD port
builds in headless mode (`ui_stub.cpp`). The wxWidgets GUI sources (`ui.cpp`,
`uibase.cpp`) are present but have not been ported to wxWidgets 3.2 / GTK3.
Use the JSON-RPC interface for all wallet and node operations.

## License

MIT/X11 — see `license.txt`.
