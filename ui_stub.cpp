// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// Headless UI stub - replaces wxWidgets GUI

#include "headers.h"
#include "sha.h"
#include "rpc.h"
#include "cluster.h"

// Stubs for GUI functions and data referenced by other code
void MainFrameRepaint() {}
map<string, string> mapAddressBook;

// Solo mining mode — skip waiting for peers
bool fSoloMine = false;

// Shutdown handling
// fShutdown declared in net.h as std::atomic<bool>
static volatile bool fRequestShutdown = false;

void HandleSignal(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    fRequestShutdown = true;
    fShutdown = true;
}

int main(int argc, char* argv[])
{
    printf("bnet v0.2.0 - headless node\n");
    printf("Based on Bitcoin 0.01 by Satoshi Nakamoto. bnet/bcash/bgold by Jacob Sitowski.\n\n");

    // Set up signal handlers
#ifndef _WIN32
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);
    signal(SIGPIPE, SIG_IGN);
#endif

    // Parse command line
    bool fGenerate = true;
    string strDataDir = "";

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "-nogenerate" || arg == "-nogen")
            fGenerate = false;
        else if (arg == "-datadir" && i + 1 < argc)
            strSetDataDir = argv[++i];
        else if (arg == "-debug")
            fDebug = true;
        else if (arg == "-solo")
            fSoloMine = true;
        else if (arg == "-help" || arg == "-h")
        {
            printf("Usage: bnet [options]\n");
            printf("Options:\n");
            printf("  -nogenerate     Don't mine blocks\n");
            printf("  -solo           Mine without peers (solo/bootstrap mode)\n");
            printf("  -datadir <dir>  Data directory\n");
            printf("  -debug          Enable debug output\n");
            printf("  -help           This help message\n");
            return 0;
        }
    }

    fGenerateBcash = fGenerate;

    // Load addresses
    printf("Loading addresses...\n");
    if (!LoadAddresses())
        printf("Warning: Could not load addresses\n");

    // Load block index
    printf("Loading block index...\n");
    if (!LoadBlockIndex())
    {
        printf("Error loading block index\n");
        return 1;
    }

    // Load wallet
    printf("Loading wallet...\n");
    if (!LoadWallet())
    {
        printf("Error loading wallet\n");
        return 1;
    }

    // Print balance
    printf("Balance: %s BC\n", FormatMoney(GetBalance()).c_str());
    printf("Block height: %d\n", nBestHeight);

    // Start node
    printf("Starting network node...\n");
    string strError;
    if (!StartNode(strError))
    {
        printf("Error: %s\n", strError.c_str());
        return 1;
    }

    // Start RPC server
    {
        pthread_t thrRPC;
        if (pthread_create(&thrRPC, NULL,
            [](void* p) -> void* { ThreadRPCServer(p); return NULL; }, NULL) != 0)
            printf("Warning: Failed to start RPC server\n");
        else
            pthread_detach(thrRPC);
    }

    // Start multi-threaded miner
    if (fGenerateBcash)
    {
        if (fSoloMine)
            printf("Starting miner in SOLO mode (no peers required)...\n");
        else
            printf("Starting miner...\n");
        StartMultiMiner();
    }

    printf("\nbnet node running. Press Ctrl+C to stop.\n");
    printf("RPC server on 127.0.0.1:9332\n\n");

    // Block until shutdown
    while (!fRequestShutdown && !fShutdown)
    {
        Sleep(1000);

        // Periodically print status
        static int64 nLastStatus = 0;
        if (GetTime() - nLastStatus > 60)
        {
            nLastStatus = GetTime();
            printf("Status: height=%d connections=%d balance=%s\n",
                nBestHeight,
                (int)vNodes.size(),
                FormatMoney(GetBalance()).c_str());
        }
    }

    // Shutdown
    printf("Shutting down...\n");
    StopNode();
    DBFlush(true);
    printf("bnet stopped.\n");
    return 0;
}
