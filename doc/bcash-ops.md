# bcash ops quick instructions

This repo is a heavily modified Zcash fork used as `bcash`.

## 1) Build

From repo root:

```bash
./autogen.sh
./configure
make -j$(sysctl -n hw.ncpu)
```

If your local workflow produces `bnet` / `bnetd` binaries at repo root, use those.
If it produces `bcash*` or `zcash*` binaries under `src/`, use those equivalents.

## 2) Minimal node config

Create `~/.bnet/bnet.conf`:

```ini
rpcport=9332
rpcallowip=127.0.0.1
listen=1
server=1
daemon=1
gen=1
genproclimit=-1
```

Optional static peers:

```ini
addnode=<peer-ip>:9333
addnode=<peer-ip>:9333
```

## 3) Start/stop

Start:

```bash
./bnetd
# or: nohup ./bnetd > ~/bnetd.out 2>&1 &
```

Stop:

```bash
pkill -f bnetd
```

## 4) Chain reset (destructive)

```bash
TS=$(date +%Y%m%d-%H%M%S)
pkill -f bnetd || true
[ -d ~/.bnet ] && mv ~/.bnet ~/.bnet.bak-$TS
mkdir -p ~/.bnet
```

Then recreate `~/.bnet/bnet.conf` and start `bnetd`.

## 5) RPC checks

```bash
curl -s --data-binary '{"jsonrpc":"1.0","id":"h","method":"getmininginfo","params":[]}' \
  -H 'content-type:text/plain;' \
  http://127.0.0.1:9332/
```

Common fields:
- `blocks`: local height
- `generate`: mining enabled
- `connections`: peer count
- `difficulty`: current target difficulty

## 6) Networking note

In this cluster, peer links may succeed over Tailscale `100.x` addresses even when LAN `192.168.x.x:9333` fails.
Use reachable addresses in `addnode`.
