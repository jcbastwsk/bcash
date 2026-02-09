// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"




//
// Global state variables
//
map<uint256, CNewsItem> mapNewsItems;
CCriticalSection cs_mapNews;



//
// HN-style ranking: score = (votes - 1) / (age_hours + 2)^1.8
//
double GetNewsScore(int nVotes, int64 nTimestamp)
{
    double dVotes = (double)(nVotes - 1);
    double dAgeHours = (double)(GetAdjustedTime() - nTimestamp) / 3600.0;
    if (dAgeHours < 0)
        dAgeHours = 0;
    return dVotes / pow(dAgeHours + 2.0, 1.8);
}



//
// Accept a new news item: verify signature, store in map, persist to CNewsDB
//
bool AcceptNewsItem(const CNewsItem& item)
{
    // Check signature
    if (!item.CheckSignature())
        return error("AcceptNewsItem() : invalid signature");

    // Title must not be empty
    if (item.strTitle.empty())
        return error("AcceptNewsItem() : empty title");

    // Must have either a URL or text body
    if (item.strURL.empty() && item.strText.empty())
        return error("AcceptNewsItem() : no URL or text");

    uint256 hash = item.GetHash();

    CRITICAL_BLOCK(cs_mapNews)
    {
        // Don't accept duplicates
        if (mapNewsItems.count(hash))
            return false;

        // Store in memory with initial vote count of 1 (submitter's implicit upvote)
        CNewsItem itemStored = item;
        itemStored.nVotes = 1;
        mapNewsItems[hash] = itemStored;
    }

    // Persist to database
    CNewsDB newsdb;
    if (!newsdb.WriteNewsItem(hash, item))
        return error("AcceptNewsItem() : WriteNewsItem failed");
    newsdb.Close();

    printf("AcceptNewsItem() : accepted %s \"%s\"\n",
        hash.ToString().substr(0, 6).c_str(),
        item.strTitle.c_str());
    return true;
}



//
// Accept a vote: verify signature, increment vote count, persist
//
bool AcceptNewsVote(const CNewsVote& vote)
{
    // Check signature
    if (!vote.CheckSignature())
        return error("AcceptNewsVote() : invalid signature");

    uint256 hashItem = vote.hashNewsItem;

    CRITICAL_BLOCK(cs_mapNews)
    {
        // News item must exist
        if (!mapNewsItems.count(hashItem))
            return error("AcceptNewsVote() : news item not found");

        // Update vote count in memory
        CNewsItem& item = mapNewsItems[hashItem];
        if (vote.fUpvote)
            item.nVotes++;
        else
            item.nVotes--;
    }

    // Persist vote to database
    CNewsDB newsdb;
    vector<CNewsVote> vVotes;
    newsdb.ReadVotes(hashItem, vVotes);
    vVotes.push_back(vote);
    if (!newsdb.WriteVotes(hashItem, vVotes))
        return error("AcceptNewsVote() : WriteVotes failed");
    newsdb.Close();

    return true;
}



//
// Get top n news items sorted by HN-style score
//
vector<CNewsItem> GetTopNews(int nCount)
{
    vector<pair<double, CNewsItem> > vScored;

    CRITICAL_BLOCK(cs_mapNews)
    {
        vScored.reserve(mapNewsItems.size());
        foreach(const PAIRTYPE(uint256, CNewsItem)& item, mapNewsItems)
        {
            double dScore = GetNewsScore(item.second.nVotes, item.second.nTime);
            vScored.push_back(make_pair(dScore, item.second));
        }
    }

    // Sort by score descending
    sort(vScored.begin(), vScored.end(),
        [](const pair<double, CNewsItem>& a, const pair<double, CNewsItem>& b) {
            return a.first > b.first;
        });

    // Return top n
    vector<CNewsItem> vResult;
    for (int i = 0; i < nCount && i < (int)vScored.size(); i++)
        vResult.push_back(vScored[i].second);
    return vResult;
}
