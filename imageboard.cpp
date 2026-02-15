// Copyright (c) 2026 bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "imageboard.h"


// Global state
map<uint256, CImagePost> mapImagePosts;
map<uint256, vector<uint256> > mapThreadReplies;
map<string, vector<uint256> > mapBoardThreads;
map<uint256, vector<unsigned char> > mapImageLibrary;
CCriticalSection cs_imageboard;


// Accept and store an image post from the network
bool AcceptImagePost(const CImagePost& post)
{
    CRITICAL_BLOCK(cs_imageboard)
    {
        uint256 hash = post.GetHash();
        if (mapImagePosts.count(hash))
            return false; // already known

        if (!post.CheckPost())
            return error("AcceptImagePost() : post failed validation");

        // Validate board name
        bool fValidBoard = false;
        for (int i = 0; pszBoards[i]; i++)
        {
            if (post.strBoard == pszBoards[i])
            {
                fValidBoard = true;
                break;
            }
        }
        if (!fValidBoard)
            return error("AcceptImagePost() : unknown board '%s'", post.strBoard.c_str());

        // Store the post
        mapImagePosts[hash] = post;

        // Store image in library if present
        if (!post.vchImage.empty())
        {
            uint256 hashImg = Hash(post.vchImage.begin(), post.vchImage.end());
            mapImageLibrary[hashImg] = post.vchImage;
        }

        // Update indices
        if (post.IsOP())
        {
            // New thread
            mapBoardThreads[post.strBoard].push_back(hash);
            mapThreadReplies[hash] = vector<uint256>(); // empty reply list
        }
        else
        {
            // Reply to existing thread
            if (mapThreadReplies.count(post.hashThread))
                mapThreadReplies[post.hashThread].push_back(hash);
            else
            {
                // Thread OP not found, create a placeholder
                mapThreadReplies[post.hashThread].push_back(hash);
            }
        }

        // Persist to database
        try
        {
            CImageDB imgdb("cr+");
            imgdb.WritePost(hash, post);

            if (post.IsOP())
                imgdb.WriteBoardIndex(post.strBoard, mapBoardThreads[post.strBoard]);

            if (mapThreadReplies.count(post.IsOP() ? hash : post.hashThread))
            {
                uint256 threadHash = post.IsOP() ? hash : post.hashThread;
                imgdb.WriteThreadIndex(threadHash, mapThreadReplies[threadHash]);
            }

            if (!post.vchImage.empty())
            {
                uint256 hashImg = Hash(post.vchImage.begin(), post.vchImage.end());
                imgdb.WriteImageData(hashImg, post.vchImage);
            }
        }
        catch (...)
        {
            printf("AcceptImagePost() : database write failed\n");
        }

        printf("AcceptImagePost() : accepted post %s on %s (trip: %s)\n",
            hash.ToString().substr(0,14).c_str(),
            post.strBoard.c_str(),
            post.GetTripcode().c_str());
    }
    return true;
}


// Create and broadcast a new image post as an on-chain transaction
bool CreateImagePost(const string& strBoard, const string& strSubject, const string& strComment,
                     const vector<unsigned char>& vchImage, const uint256& hashThread)
{
    CImagePost post;
    post.strBoard = strBoard;
    post.strSubject = strSubject;
    post.strComment = strComment;
    post.vchImage = vchImage;
    post.hashThread = hashThread;
    post.nTime = GetAdjustedTime();
    post.vchPubKey = keyUser.GetPubKey();

    if (!post.vchImage.empty())
        post.hashImage = Hash(post.vchImage.begin(), post.vchImage.end());

    CKey key;
    key.SetPubKey(keyUser.GetPubKey());
    key.SetPrivKey(mapKeys[keyUser.GetPubKey()]);
    if (!post.Sign(key))
        return error("CreateImagePost() : failed to sign post");

    // Serialize the post into a byte vector for OP_RETURN
    CDataStream ss(SER_NETWORK);
    ss << post;
    vector<unsigned char> vchData(ss.begin(), ss.end());

    // Create an on-chain transaction with OP_RETURN embedding the post
    // OP_RETURN <magic "IBRD"> <serialized post data>
    CScript scriptData;
    scriptData << OP_RETURN;
    // Prefix with "IBRD" magic bytes so we can identify imageboard txs
    vector<unsigned char> vchPayload;
    vchPayload.push_back('I');
    vchPayload.push_back('B');
    vchPayload.push_back('R');
    vchPayload.push_back('D');
    vchPayload.insert(vchPayload.end(), vchData.begin(), vchData.end());
    scriptData << vchPayload;

    // Require spendable (mature) coins to post
    if (GetBalance() == 0)
        return error("CreateImagePost() : no mature coins available (need %d confirmations)", COINBASE_MATURITY);

    CWalletTx wtx;
    int64 nFeeRequired;
    if (!CreateTransaction(scriptData, 0, wtx, nFeeRequired))
    {
        printf("CreateImagePost() : failed to create transaction (fee: %s, balance: %s)\n",
            FormatMoney(nFeeRequired).c_str(), FormatMoney(GetBalance()).c_str());
        return error("CreateImagePost() : insufficient funds for on-chain post");
    }

    if (!CommitTransactionSpent(wtx))
        return error("CreateImagePost() : failed to commit transaction");

    // Broadcast the transaction
    wtx.AcceptTransaction();
    wtx.RelayWalletTransaction();

    // Also accept the post into local imageboard state
    AcceptImagePost(post);

    printf("CreateImagePost() : on-chain tx %s for post on %s\n",
        wtx.GetHash().ToString().substr(0,14).c_str(),
        post.strBoard.c_str());

    return true;
}


// Load imageboard data from database on startup
bool LoadImageboard()
{
    try
    {
        CImageDB imgdb("r");

        // Load board indices
        for (int i = 0; pszBoards[i]; i++)
        {
            string strBoard = pszBoards[i];
            vector<uint256> vThreadHashes;
            if (imgdb.ReadBoardIndex(strBoard, vThreadHashes))
            {
                mapBoardThreads[strBoard] = vThreadHashes;

                // Load each thread's posts
                foreach(const uint256& hashOP, vThreadHashes)
                {
                    CImagePost post;
                    if (imgdb.ReadPost(hashOP, post))
                        mapImagePosts[hashOP] = post;

                    vector<uint256> vReplyHashes;
                    if (imgdb.ReadThreadIndex(hashOP, vReplyHashes))
                    {
                        mapThreadReplies[hashOP] = vReplyHashes;
                        foreach(const uint256& hashReply, vReplyHashes)
                        {
                            CImagePost reply;
                            if (imgdb.ReadPost(hashReply, reply))
                                mapImagePosts[hashReply] = reply;
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
        printf("LoadImageboard() : database load failed (may not exist yet)\n");
        return true; // not fatal
    }

    printf("LoadImageboard() : loaded %d posts across %d threads\n",
        (int)mapImagePosts.size(), (int)mapThreadReplies.size());
    return true;
}


// Get thread count for a board
int GetBoardThreadCount(const string& strBoard)
{
    CRITICAL_BLOCK(cs_imageboard)
    {
        if (mapBoardThreads.count(strBoard))
            return (int)mapBoardThreads[strBoard].size();
    }
    return 0;
}


// Get total post count for a thread
int GetThreadReplyCount(const uint256& hashThread)
{
    CRITICAL_BLOCK(cs_imageboard)
    {
        if (mapThreadReplies.count(hashThread))
            return (int)mapThreadReplies[hashThread].size();
    }
    return 0;
}


// Simple Floyd-Steinberg dithering to 16-color palette
// Input: raw RGB data (3 bytes per pixel)
// Output: indexed pixels (1 byte per pixel, palette index 0-15)
vector<unsigned char> DitherImage(const vector<unsigned char>& vchRawImage, int nWidth, int nHeight)
{
    // 16-color palette (CGA-style)
    static const unsigned char palette[16][3] = {
        {0,0,0},       {0,0,170},     {0,170,0},     {0,170,170},
        {170,0,0},     {170,0,170},   {170,170,0},   {170,170,170},
        {85,85,85},    {85,85,255},   {85,255,85},   {85,255,255},
        {255,85,85},   {255,85,255},  {255,255,85},  {255,255,255}
    };

    if ((int)vchRawImage.size() < nWidth * nHeight * 3)
        return vector<unsigned char>();

    // Working copy of image as int for error diffusion
    vector<int> img(nWidth * nHeight * 3);
    for (int i = 0; i < nWidth * nHeight * 3; i++)
        img[i] = vchRawImage[i];

    vector<unsigned char> result(nWidth * nHeight);

    for (int y = 0; y < nHeight; y++)
    {
        for (int x = 0; x < nWidth; x++)
        {
            int idx = (y * nWidth + x) * 3;
            int r = max(0, min(255, img[idx]));
            int g = max(0, min(255, img[idx+1]));
            int b = max(0, min(255, img[idx+2]));

            // Find closest palette color
            int bestDist = INT_MAX;
            int bestColor = 0;
            for (int c = 0; c < 16; c++)
            {
                int dr = r - palette[c][0];
                int dg = g - palette[c][1];
                int db = b - palette[c][2];
                int dist = dr*dr + dg*dg + db*db;
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestColor = c;
                }
            }

            result[y * nWidth + x] = bestColor;

            // Error diffusion
            int errR = r - palette[bestColor][0];
            int errG = g - palette[bestColor][1];
            int errB = b - palette[bestColor][2];

            // Floyd-Steinberg distribution
            if (x + 1 < nWidth)
            {
                int ni = idx + 3;
                img[ni]   += errR * 7 / 16;
                img[ni+1] += errG * 7 / 16;
                img[ni+2] += errB * 7 / 16;
            }
            if (y + 1 < nHeight)
            {
                if (x > 0)
                {
                    int ni = ((y+1) * nWidth + (x-1)) * 3;
                    img[ni]   += errR * 3 / 16;
                    img[ni+1] += errG * 3 / 16;
                    img[ni+2] += errB * 3 / 16;
                }
                {
                    int ni = ((y+1) * nWidth + x) * 3;
                    img[ni]   += errR * 5 / 16;
                    img[ni+1] += errG * 5 / 16;
                    img[ni+2] += errB * 5 / 16;
                }
                if (x + 1 < nWidth)
                {
                    int ni = ((y+1) * nWidth + (x+1)) * 3;
                    img[ni]   += errR * 1 / 16;
                    img[ni+1] += errG * 1 / 16;
                    img[ni+2] += errB * 1 / 16;
                }
            }
        }
    }

    return result;
}


// Simple run-length encoding compression
vector<unsigned char> CompressRLE(const vector<unsigned char>& vchData)
{
    vector<unsigned char> result;
    int i = 0;
    int n = (int)vchData.size();
    while (i < n)
    {
        unsigned char val = vchData[i];
        int count = 1;
        while (i + count < n && vchData[i + count] == val && count < 255)
            count++;
        result.push_back((unsigned char)count);
        result.push_back(val);
        i += count;
    }
    return result;
}


// RLE decompression
vector<unsigned char> DecompressRLE(const vector<unsigned char>& vchData)
{
    vector<unsigned char> result;
    for (int i = 0; i + 1 < (int)vchData.size(); i += 2)
    {
        int count = vchData[i];
        unsigned char val = vchData[i + 1];
        for (int j = 0; j < count; j++)
            result.push_back(val);
    }
    return result;
}
