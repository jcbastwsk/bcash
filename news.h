// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

class CNewsItem;
class CNewsVote;

// HN-style ranking: score = (votes - 1) / (age_hours + 2)^1.8
double GetNewsScore(int nVotes, int64 nTimestamp);

bool AcceptNewsItem(const CNewsItem& item);
bool AcceptNewsVote(const CNewsVote& vote);
vector<CNewsItem> GetTopNews(int nCount);

class CNewsItem
{
public:
    int nVersion;
    string strTitle;
    string strURL;
    string strText;
    int64 nTime;
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    // memory only
    int nVotes;

    CNewsItem()
    {
        nVersion = 1;
        nTime = 0;
        nVotes = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(strTitle);
        READWRITE(strURL);
        READWRITE(strText);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

    bool Sign(CKey& key)
    {
        return key.Sign(GetSigHash(), vchSig);
    }

    bool CheckSignature() const
    {
        return CKey::Verify(vchPubKey, GetSigHash(), vchSig);
    }
};

class CNewsVote
{
public:
    int nVersion;
    uint256 hashNewsItem;
    bool fUpvote;
    int64 nTime;
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    CNewsVote()
    {
        nVersion = 1;
        fUpvote = true;
        nTime = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashNewsItem);
        READWRITE(fUpvote);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

    bool Sign(CKey& key)
    {
        return key.Sign(GetSigHash(), vchSig);
    }

    bool CheckSignature() const
    {
        return CKey::Verify(vchPubKey, GetSigHash(), vchSig);
    }
};

// CNewsDB for persistence
class CNewsDB : public CDB
{
public:
    CNewsDB(const char* pszMode="r+", bool fTxn=false) : CDB("news.dat", pszMode, fTxn) { }
private:
    CNewsDB(const CNewsDB&);
    void operator=(const CNewsDB&);
public:
    bool WriteNewsItem(uint256 hash, const CNewsItem& item)
    {
        return Write(make_pair(string("news"), hash), item);
    }
    bool ReadNewsItem(uint256 hash, CNewsItem& item)
    {
        return Read(make_pair(string("news"), hash), item);
    }
    bool WriteVotes(uint256 hash, const vector<CNewsVote>& votes)
    {
        return Write(make_pair(string("votes"), hash), votes);
    }
    bool ReadVotes(uint256 hash, vector<CNewsVote>& votes)
    {
        return Read(make_pair(string("votes"), hash), votes);
    }
};

extern map<uint256, CNewsItem> mapNewsItems;
extern CCriticalSection cs_mapNews;
