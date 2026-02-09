// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BGOLD_H
#define BGOLD_H

class CBgoldBlock;

static const int64 BGOLD_REWARD = 10; // 10 BGOLD per block (in base units: 10 * COIN)
static const CBigNum bnBgoldProofOfWorkLimit(~uint256(0) >> 16); // easier than bcash

bool AcceptBgoldBlock(const CBgoldBlock& block);
int64 GetBgoldBalance(const vector<unsigned char>& vchPubKey);
CScript CreateBgoldCommitment(const uint256& hashBgoldBlock);




class CBgoldBlock
{
public:
    int nVersion;
    uint256 hashBcashBlock;    // the bcash block whose hash serves as PoW
    uint256 hashPrevBgoldBlock;
    int64 nTime;
    unsigned int nBits;        // bgold difficulty
    int nHeight;
    vector<unsigned char> vchPubKey; // miner's pubkey (gets the reward)

    CBgoldBlock()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashBcashBlock);
        READWRITE(hashPrevBgoldBlock);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nHeight);
        READWRITE(vchPubKey);
    )

    void SetNull()
    {
        nVersion = 1;
        hashBcashBlock = 0;
        hashPrevBgoldBlock = 0;
        nTime = 0;
        nBits = bnBgoldProofOfWorkLimit.GetCompact();
        nHeight = 0;
        vchPubKey.clear();
    }

    uint256 GetHash() const { return SerializeHash(*this); }

    string ToString() const
    {
        return strprintf("CBgoldBlock(hash=%s, ver=%d, hashBcashBlock=%s, hashPrevBgoldBlock=%s, nTime=%" PRId64 ", nBits=%08x, nHeight=%d)",
            GetHash().ToString().substr(0,14).c_str(),
            nVersion,
            hashBcashBlock.ToString().substr(0,14).c_str(),
            hashPrevBgoldBlock.ToString().substr(0,14).c_str(),
            nTime, nBits, nHeight);
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




// Bgold balance tracking
class CBgoldDB : public CDB
{
public:
    CBgoldDB(const char* pszMode="r+", bool fTxn=false) : CDB("bgold.dat", pszMode, fTxn) { }
private:
    CBgoldDB(const CBgoldDB&);
    void operator=(const CBgoldDB&);
public:
    bool WriteBgoldBlock(uint256 hash, const CBgoldBlock& block)
    {
        return Write(make_pair(string("bgold"), hash), block);
    }

    bool ReadBgoldBlock(uint256 hash, CBgoldBlock& block)
    {
        return Read(make_pair(string("bgold"), hash), block);
    }

    bool WriteBalance(uint160 hashPubKey, int64 nBalance)
    {
        return Write(make_pair(string("bgbal"), hashPubKey), nBalance);
    }

    bool ReadBalance(uint160 hashPubKey, int64& nBalance)
    {
        nBalance = 0;
        return Read(make_pair(string("bgbal"), hashPubKey), nBalance);
    }

    bool WriteHashBestBgoldBlock(uint256 hash)
    {
        return Write(string("bgoldbest"), hash);
    }

    bool ReadHashBestBgoldBlock(uint256& hash)
    {
        return Read(string("bgoldbest"), hash);
    }

    bool WriteBgoldHeight(int nHeight)
    {
        return Write(string("bgoldheight"), nHeight);
    }

    bool ReadBgoldHeight(int& nHeight)
    {
        return Read(string("bgoldheight"), nHeight);
    }
};




extern map<uint256, CBgoldBlock> mapBgoldBlocks;
extern CCriticalSection cs_bgold;
extern uint256 hashBestBgoldBlock;
extern int nBgoldHeight;

#endif
