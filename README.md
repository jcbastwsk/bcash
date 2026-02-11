# Bcash

A cryptocurrency node forked from Bitcoin 0.01 by Satoshi Nakamoto, with added
support for decentralized news, a peer-to-peer marketplace, a merge-mined
sidechain token (Bgold), and a built-in block explorer.

Runs on FreeBSD, macOS, and (with the original makefiles) Windows.

## Features

- **Proof-of-work mining** with built-in CPU miner
- **JSON-RPC interface** on `127.0.0.1:9332`
- **TUI mode** (ncurses) and **GUI mode** (wxWidgets 3.2 / GTK3)
- **News** -- submit, vote, and browse HN-style ranked news items on-chain
- **Market** -- peer-to-peer product listings with review system
- **Bgold** -- merge-mined sidechain token (10 BGOLD per block, separate difficulty)
- **Block explorer** -- zero-dependency Node.js web frontend on port 3000

## Quick start

```sh
# FreeBSD
pkg install boost-libs db5 openssl gmake
gmake -f Makefile.bsd
./bcash              # start node + miner
./bcash -solo        # bootstrap a new chain with no peers
```

```sh
# macOS (Homebrew)
brew install openssl@3 berkeley-db@5 boost
make -f Makefile.osx
./bcash
```

## Building

| Target | Makefile | Binary | Extra deps |
|--------|----------|--------|------------|
| FreeBSD TUI | `gmake -f Makefile.bsd` | `bcash` | boost-libs db5 openssl ncurses |
| FreeBSD GUI | `gmake -f Makefile.gui` | `bcash-gui` | + wx32-gtk3 |
| macOS TUI | `make -f Makefile.osx` | `bcash` | openssl@3 berkeley-db@5 boost |
| macOS GUI | `make -f Makefile.osx gui` | `bcash-gui` | + wxwidgets |

Always clean when switching between TUI and GUI builds:

```sh
gmake -f Makefile.bsd clean   # then build the other target
```

## Usage

### Command-line options

```
./bcash [options]
  -nogenerate     Don't mine blocks
  -solo           Mine without peers (solo/bootstrap mode)
  -datadir <dir>  Custom data directory (default: ~/.bcash/)
  -debug          Enable debug output
```

### JSON-RPC

```sh
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getinfo","params":[],"id":1}'
```

| Method | Params | Description |
|--------|--------|-------------|
| `getinfo` | | Node version, balance, block height, connections |
| `getbalance` | | Wallet balance in BCASH |
| `getblockcount` | | Current best block height |
| `getnewaddress` | | Generate a new receiving address |
| `sendtoaddress` | `[addr, amount]` | Send BCASH to an address |
| `getblockhash` | `[height]` | Block hash at a given height |
| `getblock` | `[hash]` | Block header and transaction list |
| `getrawtransaction` | `[txid]` | Raw transaction data |
| `getblockchaininfo` | | Chain tip, difficulty, status |
| `getrawmempool` | | Unconfirmed transaction IDs |
| `listnews` | | Top 20 news items |
| `submitnews` | `[title, url, text]` | Submit a news item |
| `votenews` | `[hash, upvote]` | Vote on a news item |
| `listproducts` | | Marketplace product listings |
| `getbgoldbalance` | | Bgold token balance |

### Block explorer

Start the node, then in a second terminal:

```sh
node explorer.js
```

Browse to `http://localhost:3000`. Routes: `/blocks`, `/block/:hash`,
`/tx/:txid`, `/search?q=`.

## Data directory

Default: `~/.bcash/`

| File | Contents |
|------|----------|
| `wallet.dat` | Private keys and wallet transactions |
| `blkindex.dat` | Block index (Berkeley DB) |
| `blk0001.dat` | Raw block data |
| `addr.dat` | Known peer addresses |

**Back up `wallet.dat` to protect your coins.**

## License

MIT/X11 -- see [license.txt](license.txt).

Based on Bitcoin 0.01 by Satoshi Nakamoto. Copyright (c) 2009 Satoshi Nakamoto.
