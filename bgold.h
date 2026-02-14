// Copyright (c) 2026 bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BGOLD_H
#define BGOLD_H

class CBgoldBlock;
class CBgoldProof;
class CBgoldBundle;

static const int64 BGOLD_REWARD = 10; // 10 BGOLD per block (in base units: 10 * COIN)
static const CBigNum bnBgoldProofOfWorkLimit(~uint256(0) >> 16); // easier than bcash


// 21e8 pattern: hash must start with bytes 0x21, 0xe8
// More leading pattern after 21e8 = more valuable
// 21e8???? (2 bytes match) = 1 unit
// 21e800?? (3 bytes) = 256 units
// 21e80000 (4 bytes) = 65536 units
inline bool Has21e8Pattern(const uint256& hash)
{
    const unsigned char* p = (const unsigned char*)&hash + 31; // big-endian first byte
    return (p[0] == 0x21 && p[-1] == 0xe8);
}

inline int64 Get21e8Value(const uint256& hash)
{
    const unsigned char* p = (const unsigned char*)&hash;
    // uint256 is stored little-endian internally, but ToString() is big-endian
    // Check big-endian: bytes [31] and [30] should be 0x21 and 0xe8
    if (p[31] != 0x21 || p[30] != 0xe8)
        return 0;

    int64 nValue = 1; // base value for 21e8 match
    // Each additional zero byte after 21e8 multiplies by 256
    for (int i = 29; i >= 0; i--)
    {
        if (p[i] == 0x00)
            nValue *= 256;
        else
            break;
    }
    return nValue;
}


// 21e8 aesthetic PoW proof
class CBgoldProof
{
public:
    int nVersion;
    vector<unsigned char> vchData;     // the content being hashed (image data, text, etc.)
    uint256 hashContent;               // SHA-256(vchData)
    unsigned int nNonce;               // nonce appended to find the pattern
    uint256 hashResult;                // SHA-256(hashContent || nonce) — must start with "21e8"
    int64 nTime;
    vector<unsigned char> vchPubKey;   // miner's pubkey
    vector<unsigned char> vchSig;

    CBgoldProof()
    {
        nVersion = 1;
        hashContent = 0;
        nNonce = 0;
        hashResult = 0;
        nTime = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(vchData);
        READWRITE(hashContent);
        READWRITE(nNonce);
        READWRITE(hashResult);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

    int64 GetValue() const { return Get21e8Value(hashResult); }

    bool CheckProof() const
    {
        // Verify hashContent = SHA-256(vchData)
        uint256 hashCheck = Hash(vchData.begin(), vchData.end());
        if (hashCheck != hashContent)
            return false;

        // Verify hashResult = SHA-256(hashContent || nonce)
        CDataStream ss;
        ss << hashContent << nNonce;
        uint256 hashVerify = Hash(ss.begin(), ss.end());
        if (hashVerify != hashResult)
            return false;

        // Verify 21e8 pattern
        if (!Has21e8Pattern(hashResult))
            return false;

        // Verify signature
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        return key.Verify(GetSigHash(), vchSig);
    }

    string ToString() const
    {
        return strprintf("CBgoldProof(hash=%s, content=%s, nonce=%u, result=%s, value=%lld)",
            GetHash().ToString().substr(0,14).c_str(),
            hashContent.ToString().substr(0,14).c_str(),
            nNonce,
            hashResult.ToString().substr(0,14).c_str(),
            GetValue());
    }
};


// Recursive reconstitution: bundle multiple proofs into a larger unit (Szabo's bitgold)
class CBgoldBundle
{
public:
    int nVersion;
    vector<uint256> vProofHashes;      // hashes of CBgoldProofs being bundled
    int64 nTotalValue;                 // sum of constituent proof values
    uint256 hashBundle;                // SHA-256 of the bundle itself
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    CBgoldBundle()
    {
        nVersion = 1;
        nTotalValue = 0;
        hashBundle = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(vProofHashes);
        READWRITE(nTotalValue);
        READWRITE(hashBundle);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
};


// Legacy merge-mined sidechain block (kept for compatibility)
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




// Bgold balance and proof tracking
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

    bool WriteBgoldProof(uint256 hash, const CBgoldProof& proof)
    {
        return Write(make_pair(string("proof"), hash), proof);
    }

    bool ReadBgoldProof(uint256 hash, CBgoldProof& proof)
    {
        return Read(make_pair(string("proof"), hash), proof);
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

    bool WriteProofCount(int nCount)
    {
        return Write(string("proofcount"), nCount);
    }

    bool ReadProofCount(int& nCount)
    {
        nCount = 0;
        return Read(string("proofcount"), nCount);
    }
};




extern map<uint256, CBgoldBlock> mapBgoldBlocks;
extern map<uint256, CBgoldProof> mapBgoldProofs;
extern CCriticalSection cs_bgold;
extern uint256 hashBestBgoldBlock;
extern int nBgoldHeight;

bool AcceptBgoldBlock(const CBgoldBlock& block);
bool AcceptBgoldProof(const CBgoldProof& proof);
int64 GetBgoldBalance(const vector<unsigned char>& vchPubKey);
CScript CreateBgoldCommitment(const uint256& hashBgoldBlock);
bool Check21e8MinerHash(const uint256& hashSingleSHA256, const vector<unsigned char>& vchPubKey,
                         unsigned int nNonce);

#endif
