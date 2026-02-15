// Copyright (c) 2026 Bcash developers
// Cluster mining via mDNS (Bonjour) — automatic LAN peer discovery
// and nonce range distribution for Apple Silicon clusters.

#ifndef BNET_CLUSTER_H
#define BNET_CLUSTER_H

#include <string>
#include <vector>
#include <map>
#include <stdint.h>

// Service type for mDNS advertisement/discovery
#define BCASH_MDNS_SERVICE_TYPE "_bnet._tcp"

// Cluster peer info (discovered via mDNS TXT record)
struct ClusterPeer
{
    std::string name;
    std::string host;
    uint16_t port;
    unsigned int ip;
    double hashrate;      // reported hash/s
    int cores;
    bool arm64;

    ClusterPeer() : port(0), ip(0), hashrate(0), cores(0), arm64(false) {}
};

// Nonce range assignment for distributed mining
struct NonceRange
{
    uint32_t nStart;
    uint32_t nEnd;
};

// Mining thread coordination
extern int nMiningThreads;
extern volatile bool fBlockFound;

// Cluster state
extern std::vector<ClusterPeer> vClusterPeers;
extern CCriticalSection cs_cluster;
extern double dLocalHashrate;
extern double dClusterHashrate;
extern int nClusterNodes;

// mDNS service management
void ClusterAdvertise(uint16_t port);
void ClusterDiscover();
void ClusterStop();

// Nonce range distribution
NonceRange GetLocalNonceRange(int nTotalPeers, int nMyIndex);
NonceRange GetThreadNonceRange(NonceRange nodeRange, int nThread, int nTotalThreads);

// Multi-threaded mining entry point
void StartMultiMiner();

#endif // BNET_CLUSTER_H
