# bcash

A peer-to-peer electronic cash system forked from Bitcoin 0.01 by Satoshi Nakamoto.

Three interlocking protocols form the **bnet** network:

- **bcash** -- currency, transactions, proof-of-work mining (SHA-256d)
- **bgold** -- 21e8 aesthetic proof-of-work; SHA-256 mini-puzzles where hashes start with `0x21e8`, merge-mined with bcash for free. Valuable vanity hashes reconstitute recursively into larger units (Nick Szabo's bit gold)
- **bnet** -- the shared P2P message layer connecting everything

## Features

- **Mining** -- CPU miner with simultaneous bcash block mining and bgold 21e8 pattern detection
- **News** -- submit, vote, and browse ranked news items on-chain
- **Marketplace** -- P2P product listings with atomic review/reputation system
- **Chess** -- play chess over payment channels with bcash bets; full move validation, ASCII board
- **Poker** -- 5-card draw with commit-reveal trustless dealing; no third party needed
- **Imageboard** -- 4chan-style boards (/b/, /g/, /biz/) with on-chain images, tripcodes, Floyd-Steinberg dithering (GUI)
- **Bgold / 21e8** -- aesthetic proof-of-work mining; find SHA-256 hashes starting with `21e8`, more trailing zeros = exponentially more valuable
- **Wallet** -- transaction details, receive addresses, WIF key export/import, confirmed/unconfirmed balance
- **JSON-RPC** -- full API on `127.0.0.1:9332`
- **Block explorer** -- zero-dependency Node.js web frontend on port 3000
- **TUI** (ncurses) and **GUI** (wxWidgets) interfaces

## Quick start

```sh
# macOS
brew install openssl@3 berkeley-db@5 boost
make -f Makefile.osx
./bcash

# FreeBSD
pkg install boost-libs db5 openssl gmake
gmake -f Makefile.bsd
./bcash
```

## Building

| Target | Command | Binary | Extra deps |
|--------|---------|--------|------------|
| FreeBSD TUI | `gmake -f Makefile.bsd` | `bcash` | boost-libs db5 openssl ncurses |
| FreeBSD GUI | `gmake -f Makefile.gui` | `bcash-gui` | + wx32-gtk3 |
| macOS TUI | `make -f Makefile.osx` | `bcash` | openssl@3 berkeley-db@5 boost |
| macOS GUI | `make -f Makefile.osx gui` | `bcash-gui` | + wxwidgets |

Clean between TUI/GUI switches: `make -f Makefile.osx clean`

## Usage

```
./bcash [options]
  -nogenerate     Don't mine
  -solo           Mine without peers (bootstrap a new chain)
  -datadir <dir>  Custom data directory (default: ~/.bcash/)
  -addnode <ip>   Connect to a specific peer
  -debug          Enable debug output
```

### TUI tabs

| Key | Tab | Description |
|-----|-----|-------------|
| 1 | WALLET | Balance, transactions, receive addresses, key export/import |
| 2 | NEWS | Submit and vote on news items |
| 3 | MARKET | Browse and list products |
| 4 | BGOLD | 21e8 proofs, bgold balance, proof history |
| 5 | SEND | Send bcash to an address |
| 6 | CHESS | Challenge opponents, play chess with bets |
| 7 | POKER | 5-card draw with payment channel betting |

### JSON-RPC

```sh
curl -s -X POST http://127.0.0.1:9332 \
  -d '{"method":"getinfo","params":[],"id":1}'
```

| Method | Params | Description |
|--------|--------|-------------|
| `getinfo` | | Node version, balance, block height, connections |
| `getbalance` | | Wallet balance |
| `getblockcount` | | Current block height |
| `getnewaddress` | | Generate a new receiving address |
| `sendtoaddress` | `[addr, amount]` | Send bcash |
| `getblockhash` | `[height]` | Block hash at height |
| `getblock` | `[hash]` | Block header and transactions |
| `getrawtransaction` | `[txid]` | Raw transaction data |
| `getblockchaininfo` | | Chain tip, difficulty |
| `getrawmempool` | | Unconfirmed transactions |
| `listnews` | | Top 20 news items |
| `submitnews` | `[title, url, text]` | Submit news |
| `votenews` | `[hash, upvote]` | Vote on news |
| `listproducts` | | Marketplace listings |
| `getbgoldbalance` | | Bgold balance |

## 21e8 / bgold

Every nonce the bcash miner tries, the intermediate single-SHA-256 hash is checked for the `0x21e8` prefix. When found, a bgold proof is created automatically -- no extra work required.

```
21e8????...  = 1 bgold unit
21e800??...  = 256 units
21e80000...  = 65,536 units
```

Proofs can be bundled recursively into larger units, implementing Szabo's bit gold concept.

## Data directory

Default: `~/.bcash/`

| File | Contents |
|------|----------|
| `wallet.dat` | Private keys and transactions |
| `blkindex.dat` | Block index |
| `blk0001.dat` | Raw block data |
| `bgold.dat` | Bgold proofs and balances |
| `game.dat` | Chess/poker game sessions |
| `imageboard.dat` | Imageboard posts and images |
| `addr.dat` | Known peer addresses |

**Back up `wallet.dat` to protect your coins.**

## License

MIT/X11 -- see [license.txt](license.txt).

Based on Bitcoin 0.01 by Satoshi Nakamoto.
