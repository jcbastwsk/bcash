// Copyright (c) 2026 Bcash developers
// Cluster mining via mDNS (Bonjour) — automatic LAN peer discovery
// and nonce range distribution for Apple Silicon clusters.

#include "headers.h"
#include "cluster.h"

#ifdef __APPLE__
#include <dns_sd.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <unistd.h>
#include <thread>
#include <atomic>

// Global cluster state
std::vector<ClusterPeer> vClusterPeers;
CCriticalSection cs_cluster;
double dLocalHashrate = 0;
double dClusterHashrate = 0;
int nClusterNodes = 0;
int nMiningThreads = 0;  // 0 = auto-detect
volatile bool fBlockFound = false;

#ifdef __APPLE__

static DNSServiceRef g_sdRef = NULL;
static DNSServiceRef g_browseRef = NULL;
static std::atomic<bool> fClusterRunning(false);

// Callback when our service is registered
static void DNSSD_API RegisterCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags,
    DNSServiceErrorType errorCode, const char *name,
    const char *regtype, const char *domain, void *context)
{
    if (errorCode == kDNSServiceErr_NoError)
        printf("mDNS: registered service '%s' on %s%s\n", name, regtype, domain);
    else
        printf("mDNS: registration failed (%d)\n", errorCode);
}

// Build TXT record with our capabilities
static std::vector<unsigned char> BuildTXTRecord()
{
    char buf[256];
    std::vector<unsigned char> txt;

    auto addField = [&](const char* kv) {
        int len = strlen(kv);
        txt.push_back((unsigned char)len);
        txt.insert(txt.end(), kv, kv + len);
    };

    snprintf(buf, sizeof(buf), "version=%d", VERSION);
    addField(buf);

    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    snprintf(buf, sizeof(buf), "cores=%d", cores);
    addField(buf);

    snprintf(buf, sizeof(buf), "hashrate=%.0f", dLocalHashrate);
    addField(buf);

#if defined(__aarch64__)
    addField("arm64=1");
#else
    addField("arm64=0");
#endif

    return txt;
}

void ClusterAdvertise(uint16_t port)
{
    if (g_sdRef) return;

    std::vector<unsigned char> txt = BuildTXTRecord();

    DNSServiceErrorType err = DNSServiceRegister(
        &g_sdRef,
        0,                          // flags
        0,                          // interface index (all)
        NULL,                       // name (auto)
        BCASH_MDNS_SERVICE_TYPE,
        NULL,                       // domain (default)
        NULL,                       // host (default)
        htons(port),
        txt.size(), txt.data(),
        RegisterCallback,
        NULL                        // context
    );

    if (err != kDNSServiceErr_NoError)
    {
        printf("mDNS: DNSServiceRegister failed (%d)\n", err);
        g_sdRef = NULL;
        return;
    }

    fClusterRunning = true;

    // Process mDNS events in background
    std::thread([]{
        while (fClusterRunning && g_sdRef)
        {
            int fd = DNSServiceRefSockFD(g_sdRef);
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval tv = { 1, 0 };
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0)
                DNSServiceProcessResult(g_sdRef);
        }
    }).detach();
}

// Forward declaration
static void DNSSD_API ResolveCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags,
    uint32_t interfaceIndex, DNSServiceErrorType errorCode,
    const char *fullname, const char *hosttarget, uint16_t port,
    uint16_t txtLen, const unsigned char *txtRecord, void *context);

// Callback when a peer is found/lost
static void DNSSD_API BrowseCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags,
    uint32_t interfaceIndex, DNSServiceErrorType errorCode,
    const char *serviceName, const char *regtype,
    const char *replyDomain, void *context)
{
    if (errorCode != kDNSServiceErr_NoError)
        return;

    if (flags & kDNSServiceFlagsAdd)
    {
        printf("mDNS: found peer '%s'\n", serviceName);
        // Resolve to get IP/port
        DNSServiceRef resolveRef = NULL;
        DNSServiceResolve(&resolveRef, 0, interfaceIndex,
            serviceName, regtype, replyDomain,
            ResolveCallback, NULL);

        if (resolveRef)
        {
            // Process synchronously (one result)
            int fd = DNSServiceRefSockFD(resolveRef);
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval tv = { 5, 0 };
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0)
                DNSServiceProcessResult(resolveRef);
            DNSServiceRefDeallocate(resolveRef);
        }
    }
    else
    {
        printf("mDNS: peer '%s' left\n", serviceName);
        CRITICAL_BLOCK(cs_cluster)
        {
            for (auto it = vClusterPeers.begin(); it != vClusterPeers.end(); ++it)
            {
                if (it->name == serviceName)
                {
                    vClusterPeers.erase(it);
                    break;
                }
            }
            nClusterNodes = vClusterPeers.size();
        }
    }
}

// Parse TXT record fields
static std::map<std::string, std::string> ParseTXTRecord(uint16_t txtLen, const unsigned char *txtRecord)
{
    std::map<std::string, std::string> fields;
    const unsigned char *p = txtRecord;
    const unsigned char *end = txtRecord + txtLen;

    while (p < end)
    {
        int len = *p++;
        if (p + len > end) break;
        std::string kv((const char*)p, len);
        p += len;

        size_t eq = kv.find('=');
        if (eq != std::string::npos)
            fields[kv.substr(0, eq)] = kv.substr(eq + 1);
    }
    return fields;
}

static void DNSSD_API ResolveCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags,
    uint32_t interfaceIndex, DNSServiceErrorType errorCode,
    const char *fullname, const char *hosttarget, uint16_t port,
    uint16_t txtLen, const unsigned char *txtRecord, void *context)
{
    if (errorCode != kDNSServiceErr_NoError)
        return;

    printf("mDNS: resolved '%s' -> %s:%u\n", fullname, hosttarget, ntohs(port));

    // Parse TXT record
    auto fields = ParseTXTRecord(txtLen, txtRecord);

    ClusterPeer peer;
    peer.name = fullname;
    peer.host = hosttarget;
    peer.port = port;  // already in network byte order
    if (fields.count("hashrate"))
        peer.hashrate = atof(fields["hashrate"].c_str());
    if (fields.count("cores"))
        peer.cores = atoi(fields["cores"].c_str());
    if (fields.count("arm64"))
        peer.arm64 = atoi(fields["arm64"].c_str());

    // Resolve hostname to IP for ConnectNode
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hosttarget, NULL, &hints, &res) == 0 && res)
    {
        struct sockaddr_in *sinp = (struct sockaddr_in*)res->ai_addr;
        peer.ip = sinp->sin_addr.s_addr;
        freeaddrinfo(res);

        // Add to cluster peers
        CRITICAL_BLOCK(cs_cluster)
        {
            // Don't add ourselves
            if (peer.ip != addrLocalHost.ip)
            {
                // Update or add
                bool found = false;
                for (auto &p : vClusterPeers)
                {
                    if (p.ip == peer.ip)
                    {
                        p = peer;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    vClusterPeers.push_back(peer);

                nClusterNodes = vClusterPeers.size();
                printf("mDNS: cluster now has %d peers\n", nClusterNodes);

                // Also connect via P2P if not already connected
                CAddress addr(peer.ip, peer.port, NODE_NETWORK);
                if (!FindNode(peer.ip))
                {
                    CNode* pnode = ConnectNode(addr);
                    if (pnode)
                    {
                        pnode->fNetworkNode = true;
                        printf("mDNS: connected to peer %s\n", addr.ToString().c_str());
                    }
                }
            }
        }
    }
    else
    {
        if (res) freeaddrinfo(res);
    }
}

void ClusterDiscover()
{
    if (g_browseRef) return;

    DNSServiceErrorType err = DNSServiceBrowse(
        &g_browseRef,
        0,                          // flags
        0,                          // interface index (all)
        BCASH_MDNS_SERVICE_TYPE,
        NULL,                       // domain (default)
        BrowseCallback,
        NULL                        // context
    );

    if (err != kDNSServiceErr_NoError)
    {
        printf("mDNS: DNSServiceBrowse failed (%d)\n", err);
        g_browseRef = NULL;
        return;
    }

    // Process browse events in background
    std::thread([]{
        while (fClusterRunning && g_browseRef)
        {
            int fd = DNSServiceRefSockFD(g_browseRef);
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval tv = { 1, 0 };
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0)
                DNSServiceProcessResult(g_browseRef);
        }
    }).detach();
}

void ClusterStop()
{
    fClusterRunning = false;
    if (g_sdRef)
    {
        DNSServiceRefDeallocate(g_sdRef);
        g_sdRef = NULL;
    }
    if (g_browseRef)
    {
        DNSServiceRefDeallocate(g_browseRef);
        g_browseRef = NULL;
    }
}

#else
// Non-Apple stubs
void ClusterAdvertise(uint16_t port) { }
void ClusterDiscover() { }
void ClusterStop() { }
#endif


//
// Nonce range distribution
//

NonceRange GetLocalNonceRange(int nTotalPeers, int nMyIndex)
{
    NonceRange range;
    if (nTotalPeers <= 1)
    {
        range.nStart = 0;
        range.nEnd = 0xFFFFFFFF;
        return range;
    }

    uint64_t nTotal = 0x100000000ULL;
    uint64_t nPerPeer = nTotal / nTotalPeers;

    range.nStart = (uint32_t)(nPerPeer * nMyIndex);
    if (nMyIndex == nTotalPeers - 1)
        range.nEnd = 0xFFFFFFFF;
    else
        range.nEnd = (uint32_t)(nPerPeer * (nMyIndex + 1) - 1);

    return range;
}

NonceRange GetThreadNonceRange(NonceRange nodeRange, int nThread, int nTotalThreads)
{
    NonceRange range;
    uint64_t nTotal = (uint64_t)nodeRange.nEnd - nodeRange.nStart + 1;
    uint64_t nPerThread = nTotal / nTotalThreads;

    range.nStart = nodeRange.nStart + (uint32_t)(nPerThread * nThread);
    if (nThread == nTotalThreads - 1)
        range.nEnd = nodeRange.nEnd;
    else
        range.nEnd = nodeRange.nStart + (uint32_t)(nPerThread * (nThread + 1) - 1);

    return range;
}
