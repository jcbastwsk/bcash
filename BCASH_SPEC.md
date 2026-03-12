# BCASH - Zcash Fork Specification

## Overview

BCASH is a Zcash fork that implements the following modifications for a proof-of-work blockchain designed for hardware-diverse mining, radio-native transaction relay, and modular scalability.

## Key Modifications from Zcash

### 1. Consensus Parameters

**File: `src/consensus/consensus.h`**
- `MAX_BLOCK_SIZE`: Changed from 2,000,000 to 300,000 bytes (300 KB)
- Added `MAX_SUPPLY`: 10,000,000 BCASH

**File: `src/consensus/params.h`**
- `PRE_BLOSSOM_POW_TARGET_SPACING`: Changed from 150 to 120 seconds
- `POST_BLOSSOM_POW_TARGET_SPACING`: Changed from 75 to 120 seconds (no Blossom)
- `BLOSSOM_POW_TARGET_SPACING_RATIO`: Set to 1 (no Blossom upgrade)
- `PRE_BLOSSOM_REGTEST_HALVING_INTERVAL`: Changed from 144 to 210,000 blocks

**File: `src/consensus/params.cpp`**
- `GetBlockSubsidy`: Changed initial reward from 12.5 to 5 BCASH

### 2. MetalGraph-21 Proof-of-Work

**New Files:**
- `src/crypto/metalgraph21.h` - MetalGraph-21 algorithm header
- `src/crypto/metalgraph21.cpp` - MetalGraph-21 PoW algorithm implementation

**Features:**
- Two-stage PoW system:
  1. 21e8 Mini-PoW filter (1 in 65,536 probability)
  2. Memory-bandwidth bound MetalGraph-21 computation
- Parameters:
  - Nodes: 2^27 (134,217,728)
  - Edges per node: 8
  - Memory requirement: ~4 GB

**Modified Files:**
- `src/pow.h` - Added MetalGraph-21 and Check21e8MiniPow declarations
- `src/pow.cpp` - Added Check21e8MiniPow implementation

### 3. Web-of-Trust Identity Layer

**Modified Files:**
- `src/primitives/block.h`
  - Added `hashTrustRoot` field to CBlockHeader (32 bytes)
  - Updated HEADER_SIZE to include trust root
  - Updated serialization to include trust root
  - Updated GetBlockHeader to copy trust root

**Constants:**
- `BCASH_WOT_ENABLED`: Identity layer enabled
- Trust value precision: 0.01 (100x)
- Max trust path length: 10
- Minimum trust vouches: 3

### 4. Radio Relay Support

**New Files:**
- `src/net_radio.h` - Radio relay header
- `src/net_radio.cpp` - Radio relay implementation

**Features:**
- Support for multiple radio types:
  - HF packet radio (3-30 MHz)
  - VHF amateur radio (30-300 MHz)
  - LoRa mesh (433/868/915 MHz)
  - Satellite broadcast
- Forward Error Correction (FEC)
- Compact block propagation
- Store-and-forward for high-latency networks
- Transmission time estimation

## Block Parameters

| Parameter | Value |
|-----------|-------|
| Target Block Time | 120 seconds |
| Max Block Size | 300 KB |
| Initial Block Reward | 5 BCASH |
| Halving Interval | 210,000 blocks |
| Max Supply | 10,000,000 BCASH |

## Emission Schedule

- Initial reward: 5 BCASH per block
- Halving: Every 210,000 blocks (~291 days at 120s blocks)
- Total supply: 10,000,000 BCASH (asymptotically)

## Hardware Bias

BCASH mining is designed to favor consumer hardware:
- Memory-bandwidth bound (favors CPUs/GPUs with high memory bandwidth)
- Unified memory architectures (like Apple Silicon)
- No ASIC advantage due to memory requirements

## Build Instructions

```bash
# Generate configure script
./autogen.sh

# Configure
./configure

# Build
make -j$(nproc)

# Install
sudo make install
```

## Network Ports (Default)

- Mainnet P2P: 9333
- Mainnet RPC: 9332
- Testnet/Regtest: project-specific (set in runtime config)

## License

MIT License - See COPYING file
