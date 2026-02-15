# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

macOS (Apple Silicon). Clean between target switches.

```sh
# Dependencies
brew install openssl@3 berkeley-db@5 boost wxwidgets

# TUI (ncurses)
make -f Makefile.osx

# Headless daemon
make -f Makefile.osx bnetd

# GUI (wxWidgets)
make -f Makefile.osx gui

# Clean
make -f Makefile.osx clean
```

Binaries: `bnet` (TUI), `bnetd` (daemon), `bnet-gui` (GUI).

FreeBSD: `gmake -f Makefile.bsd` / `gmake -f Makefile.gui`.

```sh
# Run
./bnetd -solo          # solo mine, no peers
./bnet -addnode <ip>   # connect to specific peer
./bnet -nogenerate     # don't mine
```

## Architecture

Bitcoin 0.01 fork (Satoshi's original C++ codebase, ~2009 style). Single `headers.h` precompiled header includes everything. Uses `CRITICAL_BLOCK(cs_*)` mutex macros, `foreach` (boost), `loop` (infinite loop macro), `int64` typedef.

### Core layers

| Layer | Files | Description |
|-------|-------|-------------|
| Consensus | `main.cpp`, `main.h` | Blocks, transactions, mining, wallet, SelectCoins, ConnectBlock |
| Crypto | `sha.cpp`, `sha256_arm64.cpp`, `key.h`, `bignum.h` | SHA-256d PoW, ECDSA signing, ARM64 hardware acceleration |
| Network | `net.cpp`, `net.h`, `irc.cpp` | P2P messaging, peer discovery, IRC bootstrap |
| Storage | `db.cpp`, `db.h` | Berkeley DB 5 persistence (wallet.dat, blkindex.dat, etc.) |
| Script | `script.cpp`, `script.h` | Bitcoin script engine, IsMine() |
| RPC | `rpc.cpp`, `rpc.h` | JSON-RPC on port 9332, **also contains the entire web dashboard** (inline HTML/CSS/JS) |
| Serialization | `serialize.h` | CDataStream, IMPLEMENT_SERIALIZE macros |
| Utilities | `util.cpp`, `util.h`, `uint256.h`, `base58.h` | Logging, time, address encoding |

### Feature modules

| Module | Files | On-chain? |
|--------|-------|-----------|
| Bgold/21e8 | `bgold.cpp`, `bgold.h` | Yes — merge-mined during SHA-256d, stored in bgold.dat |
| News | `news.cpp`, `news.h` | Yes — OP_RETURN with "NEWS" prefix |
| Marketplace | `market.cpp`, `market.h` | Yes — OP_RETURN with "MRKT" prefix |
| Chess | `chess.cpp`, `chess.h`, `gamechannel.cpp` | Payment channels |
| Poker | `poker.cpp`, `poker.h`, `gamechannel.cpp` | Payment channels, commit-reveal |
| Imageboard | `imageboard.cpp`, `imageboard.h` | Yes — OP_RETURN with "IBRD" prefix, requires mature coins |
| Cluster | `cluster.cpp`, `cluster.h` | No — local multi-machine coordination |

### UI layers

| Target | Files | Notes |
|--------|-------|-------|
| TUI | `ui_tui.cpp` | ncurses, tabs 1-7 |
| GUI | `ui.cpp`, `uibase.cpp`, `uibase.h`, `ui.h`, `osx_helper.mm` | wxWidgets, `-DGUI` flag |
| Daemon | `ui_stub.cpp` | No UI, RPC + web dashboard only |
| Web dashboard | embedded in `rpc.cpp` | Served on RPC port 9332, auto-refresh via JS polling |

## Key constants (main.h)

- `COINBASE_MATURITY = 100` — coinbase rewards need 100 confirmations before spending
- `COIN = 100000000` (1e8 satoshis)
- P2P port: **9333**, RPC port: **9332**
- Data directory: `~/.bcash/` (wallet.dat, blk0001.dat, bgold.dat, game.dat, imageboard.dat, addr.dat)

## Known gotchas

- `SelectCoins(0, ...)` returns true with an empty coin set (0 < 0 is false). Guard with `GetBalance() == 0` before calling CreateTransaction with zero value.
- `CMerkleTx::GetCredit()` returns 0 for immature coinbase — wallet balance shows 0 until chain height > COINBASE_MATURITY.
- Seed peers in addr.dat may be RFC 5737 TEST-NET-3 addresses (203.0.113.x). Use `-addnode` with real IPs.
- `CNode` has no byte counter fields — don't reference `bytessent`/`bytesrecv` in RPC or UI.
- Web dashboard JS lives inline in rpc.cpp string literals. Changes require rebuild.
- `rpc.cpp` is ~3500+ lines — contains HTTP server, all RPC handlers, and the full web UI.

## GUI redesign brief

The GUI needs to be redesigned from Satoshi's original 2009 wxWidgets stock widgets.

### Aesthetic
- **Dark theme** — black/near-black backgrounds, high contrast text
- **Accent color**: amber/gold (#FFD700) — 21e8 aesthetic, Bloomberg terminal energy
- **Monospace** for hashes, addresses, amounts; **proportional** for labels and prose
- **Dense, information-rich** — power-user software, not consumer app

### Architecture
- Stay with **wxWidgets** (no Qt/Electron rewrite)
- Replace FormBuilder-generated layouts (uiproject.fbp / uibase.cpp) with hand-coded layouts
- Use wxDC custom drawing for dark theming
- Don't break the TUI (`ui_tui.cpp`) or any backend logic

### Tabs
1. **WALLET** — balance, transactions, receive addresses, WIF export/import
2. **NEWS** — ranked items, submit form, voting
3. **MARKET** — product listings, detail view, reviews
4. **BGOLD/21e8** — balance, proof history, mining indicator, visual hash display
5. **SEND** — address, amount, fee, send button (amber accent)
6. **CHESS** — graphical 8x8 board with Unicode pieces, move history, bets
7. **POKER** — card rendering, bet/fold/call, pot, commit-reveal status
8. **IMAGEBOARD** — board selector, thread list with thumbnails, post form

### Status bar
Block height, peer count, mining hashrate, 21e8 search rate, bgold balance, network status.

### Priority order
1. Dark theme foundation
2. Replace uibase.cpp with hand-coded layouts
3. Wallet tab → Status bar → Bgold → News/Market → Send → Chess → Poker → Imageboard

## Rules
- Compile after every significant change. Fix errors before moving on.
- Don't break the TUI or any backend logic.
- Git commit after each working milestone.
