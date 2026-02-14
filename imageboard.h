// Copyright (c) 2026 bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef IMAGEBOARD_H
#define IMAGEBOARD_H

class CImagePost;
class CImageDB;

// A single imageboard post (OP or reply)
class CImagePost
{
public:
    int nVersion;
    uint256 hashThread;             // 0 for new thread (OP), otherwise hash of OP
    string strBoard;                // board name: "/b/", "/g/", "/biz/"
    string strSubject;              // thread subject (OP only)
    string strComment;              // post text
    vector<unsigned char> vchImage; // dithered/compressed image data (or empty)
    uint256 hashImage;              // hash of image (for reuse)
    int64 nTime;
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    CImagePost()
    {
        nVersion = 1;
        hashThread = 0;
        hashImage = 0;
        nTime = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashThread);
        READWRITE(strBoard);
        READWRITE(strSubject);
        READWRITE(strComment);
        READWRITE(vchImage);
        READWRITE(hashImage);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

    bool IsOP() const { return hashThread == 0; }

    // Tripcode: Hash160(pubkey) base58 truncated to 8 chars
    string GetTripcode() const
    {
        uint160 hash = Hash160(vchPubKey);
        // Encode in base58 and take first 8 chars
        vector<unsigned char> vch(UBEGIN(hash), UEND(hash));
        string strFull = EncodeBase58(vch);
        string strTrip = "!";
        if (strFull.size() >= 8)
            strTrip += strFull.substr(0, 8);
        else
            strTrip += strFull;
        return strTrip;
    }

    bool CheckSignature() const
    {
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        return key.Verify(GetSigHash(), vchSig);
    }

    bool Sign(CKey& key)
    {
        return key.Sign(GetSigHash(), vchSig);
    }

    bool CheckPost() const
    {
        // Basic validation
        if (strBoard.empty())
            return false;
        if (strComment.empty() && vchImage.empty())
            return false;
        if (vchImage.size() > 256 * 1024) // 256KB max
            return false;
        if (strComment.size() > 4096) // 4KB max comment
            return false;
        if (vchPubKey.empty())
            return false;
        return CheckSignature();
    }

    string ToString() const
    {
        return strprintf("CImagePost(board=%s, thread=%s, subject=%s, time=%lld, trip=%s)",
            strBoard.c_str(),
            hashThread.ToString().substr(0,14).c_str(),
            strSubject.substr(0,20).c_str(),
            nTime,
            GetTripcode().c_str());
    }
};


// Image database
class CImageDB : public CDB
{
public:
    CImageDB(const char* pszMode="r+", bool fTxn=false) : CDB("imageboard.dat", pszMode, fTxn) { }
private:
    CImageDB(const CImageDB&);
    void operator=(const CImageDB&);
public:
    bool WritePost(uint256 hash, const CImagePost& post)
    {
        return Write(make_pair(string("post"), hash), post);
    }

    bool ReadPost(uint256 hash, CImagePost& post)
    {
        return Read(make_pair(string("post"), hash), post);
    }

    bool WriteThreadIndex(uint256 hashThread, const vector<uint256>& vPostHashes)
    {
        return Write(make_pair(string("thread"), hashThread), vPostHashes);
    }

    bool ReadThreadIndex(uint256 hashThread, vector<uint256>& vPostHashes)
    {
        return Read(make_pair(string("thread"), hashThread), vPostHashes);
    }

    bool WriteBoardIndex(const string& strBoard, const vector<uint256>& vThreadHashes)
    {
        return Write(make_pair(string("board"), strBoard), vThreadHashes);
    }

    bool ReadBoardIndex(const string& strBoard, vector<uint256>& vThreadHashes)
    {
        return Read(make_pair(string("board"), strBoard), vThreadHashes);
    }

    bool WriteImageData(uint256 hash, const vector<unsigned char>& vchData)
    {
        return Write(make_pair(string("img"), hash), vchData);
    }

    bool ReadImageData(uint256 hash, vector<unsigned char>& vchData)
    {
        return Read(make_pair(string("img"), hash), vchData);
    }
};


// Hardcoded boards
static const char* pszBoards[] = { "/b/", "/g/", "/biz/", NULL };

// Globals
extern map<uint256, CImagePost> mapImagePosts;
extern map<uint256, vector<uint256> > mapThreadReplies;  // thread hash -> reply hashes
extern map<string, vector<uint256> > mapBoardThreads;    // board name -> OP hashes
extern map<uint256, vector<unsigned char> > mapImageLibrary;
extern CCriticalSection cs_imageboard;

// Functions
bool AcceptImagePost(const CImagePost& post);
bool CreateImagePost(const string& strBoard, const string& strSubject, const string& strComment,
                     const vector<unsigned char>& vchImage, const uint256& hashThread = 0);

// Simple Floyd-Steinberg dithering to reduce image to indexed palette
vector<unsigned char> DitherImage(const vector<unsigned char>& vchRawImage, int nWidth, int nHeight);

// Simple RLE compression for indexed image data
vector<unsigned char> CompressRLE(const vector<unsigned char>& vchData);
vector<unsigned char> DecompressRLE(const vector<unsigned char>& vchData);

#endif
