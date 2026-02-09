// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "sha.h"
#include "bgold.h"




//
// Global state
//

map<uint256, CBgoldBlock> mapBgoldBlocks;
CCriticalSection cs_bgold;
uint256 hashBestBgoldBlock = 0;
int nBgoldHeight = 0;




//////////////////////////////////////////////////////////////////////////////
//
// Bgold merge-mined block acceptance
//

bool AcceptBgoldBlock(const CBgoldBlock& block)
{
    // Verify the referenced bcash block exists in mapBlockIndex
    CRITICAL_BLOCK(cs_main)
    {
        if (mapBlockIndex.find(block.hashBcashBlock) == mapBlockIndex.end())
            return error("AcceptBgoldBlock() : hashBcashBlock not found in mapBlockIndex");

        CBlockIndex* pindex = mapBlockIndex[block.hashBcashBlock];
        if (!pindex->IsInMainChain())
            return error("AcceptBgoldBlock() : bcash block is not in main chain");
    }

    // Check that the bcash block's hash meets the bgold difficulty target.
    // The bcash block hash serves as the proof-of-work for bgold.
    CBigNum bnTarget;
    bnTarget.SetCompact(block.nBits);
    if (bnTarget <= 0 || bnTarget > bnBgoldProofOfWorkLimit)
        return error("AcceptBgoldBlock() : nBits below minimum work");

    // Get the bcash block hash and verify it meets bgold's difficulty
    uint256 hashBcash;
    CRITICAL_BLOCK(cs_main)
    {
        CBlockIndex* pindex = mapBlockIndex[block.hashBcashBlock];
        hashBcash = pindex->GetBlockHash();
    }

    if (CBigNum(hashBcash) > bnTarget)
        return error("AcceptBgoldBlock() : bcash block hash does not meet bgold difficulty target");

    CRITICAL_BLOCK(cs_bgold)
    {
        // Verify hashPrevBgoldBlock is the current best bgold block
        if (block.hashPrevBgoldBlock != hashBestBgoldBlock)
            return error("AcceptBgoldBlock() : hashPrevBgoldBlock does not match current best");

        // Verify the block height is correct
        if (block.nHeight != nBgoldHeight + 1)
            return error("AcceptBgoldBlock() : incorrect block height");

        // Check that the block is not already known
        uint256 hashBgold = block.GetHash();
        if (mapBgoldBlocks.count(hashBgold))
            return error("AcceptBgoldBlock() : block already known");

        // Verify the miner's pubkey is present
        if (block.vchPubKey.empty())
            return error("AcceptBgoldBlock() : empty miner pubkey");

        // Credit BGOLD_REWARD * COIN to the miner's pubkey
        uint160 hashPubKey = Hash160(block.vchPubKey);
        int64 nBalance = 0;

        CBgoldDB bgolddb;
        bgolddb.ReadBalance(hashPubKey, nBalance);
        nBalance += BGOLD_REWARD * COIN;

        // Persist the block and updated balance
        if (!bgolddb.TxnBegin())
            return error("AcceptBgoldBlock() : TxnBegin failed");

        if (!bgolddb.WriteBgoldBlock(hashBgold, block))
        {
            bgolddb.TxnAbort();
            return error("AcceptBgoldBlock() : WriteBgoldBlock failed");
        }

        if (!bgolddb.WriteBalance(hashPubKey, nBalance))
        {
            bgolddb.TxnAbort();
            return error("AcceptBgoldBlock() : WriteBalance failed");
        }

        if (!bgolddb.WriteHashBestBgoldBlock(hashBgold))
        {
            bgolddb.TxnAbort();
            return error("AcceptBgoldBlock() : WriteHashBestBgoldBlock failed");
        }

        if (!bgolddb.WriteBgoldHeight(block.nHeight))
        {
            bgolddb.TxnAbort();
            return error("AcceptBgoldBlock() : WriteBgoldHeight failed");
        }

        if (!bgolddb.TxnCommit())
            return error("AcceptBgoldBlock() : TxnCommit failed");

        // Update in-memory state
        mapBgoldBlocks[hashBgold] = block;
        hashBestBgoldBlock = hashBgold;
        nBgoldHeight = block.nHeight;
    }

    printf("AcceptBgoldBlock() : accepted bgold block %s at height %d\n",
        block.GetHash().ToString().substr(0,14).c_str(), block.nHeight);
    return true;
}




//////////////////////////////////////////////////////////////////////////////
//
// Bgold balance query
//

int64 GetBgoldBalance(const vector<unsigned char>& vchPubKey)
{
    uint160 hashPubKey = Hash160(vchPubKey);
    int64 nBalance = 0;
    CBgoldDB bgolddb("r");
    bgolddb.ReadBalance(hashPubKey, nBalance);
    return nBalance;
}




//////////////////////////////////////////////////////////////////////////////
//
// Helper to create bgold commitment for coinbase
//
// Creates an OP_RETURN script containing the bgold block hash.
// The miner embeds this in the coinbase transaction of the bcash block
// to commit to a bgold block.
//

CScript CreateBgoldCommitment(const uint256& hashBgoldBlock)
{
    CScript script;
    script << OP_RETURN;

    // Prefix "BGLD" as a 4-byte tag so parsers can identify bgold commitments
    vector<unsigned char> vchData;
    vchData.push_back('B');
    vchData.push_back('G');
    vchData.push_back('L');
    vchData.push_back('D');

    // Append the bgold block hash (32 bytes)
    const unsigned char* pbegin = (const unsigned char*)&hashBgoldBlock;
    const unsigned char* pend = pbegin + sizeof(hashBgoldBlock);
    vchData.insert(vchData.end(), pbegin, pend);

    script << vchData;
    return script;
}
