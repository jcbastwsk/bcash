// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "sha.h"
#include "bgold.h"
#include "rpc.h"
#include "cluster.h"
#include "imageboard.h"


///
/// Simple JSON helpers - no external JSON library dependency
///

string JSONValue(const string& val)
{
    // Escape special characters
    string escaped;
    foreach(char c, val)
    {
        if (c == '"')        escaped += "\\\"";
        else if (c == '\\')  escaped += "\\\\";
        else if (c == '\n')  escaped += "\\n";
        else if (c == '\r')  escaped += "\\r";
        else if (c == '\t')  escaped += "\\t";
        else                 escaped += c;
    }
    return "\"" + escaped + "\"";
}

string JSONValue(int64 val)
{
    return strprintf("%" PRId64, val);
}

string JSONValue(int val)
{
    return strprintf("%d", val);
}

string JSONValue(double val)
{
    return strprintf("%.8f", val);
}

string JSONBool(bool val)
{
    return val ? "true" : "false";
}

string JSONResult(const string& result, const string& id)
{
    return "{\"result\":" + result + ",\"error\":null,\"id\":" + id + "}\n";
}

string JSONError(const string& msg, const string& id)
{
    return "{\"result\":null,\"error\":{\"message\":" + JSONValue(msg) + "},\"id\":" + id + "}\n";
}


///
/// Parse a simple JSON-RPC request - extract method, params, and id.
/// Very basic string-find approach: locate "method":"xxx", "params":[...], "id":xxx
///
bool ParseRPCRequest(const string& strRequest, string& strMethod, string& strParams, string& strId)
{
    // Find "method"
    size_t pos = strRequest.find("\"method\"");
    if (pos == string::npos)
        return false;

    // Skip past "method" and colon
    pos = strRequest.find(':', pos + 8);
    if (pos == string::npos)
        return false;
    pos++;

    // Skip whitespace
    while (pos < strRequest.size() && (strRequest[pos] == ' ' || strRequest[pos] == '\t'))
        pos++;

    // Expect opening quote
    if (pos >= strRequest.size() || strRequest[pos] != '"')
        return false;
    pos++;

    // Read method name until closing quote
    size_t end = strRequest.find('"', pos);
    if (end == string::npos)
        return false;
    strMethod = strRequest.substr(pos, end - pos);

    // Find "params"
    strParams = "[]";
    pos = strRequest.find("\"params\"");
    if (pos != string::npos)
    {
        pos = strRequest.find(':', pos + 8);
        if (pos != string::npos)
        {
            pos++;
            while (pos < strRequest.size() && (strRequest[pos] == ' ' || strRequest[pos] == '\t'))
                pos++;

            if (pos < strRequest.size() && strRequest[pos] == '[')
            {
                // Find matching closing bracket
                int depth = 0;
                size_t start = pos;
                for (size_t i = pos; i < strRequest.size(); i++)
                {
                    if (strRequest[i] == '[') depth++;
                    else if (strRequest[i] == ']') depth--;
                    if (depth == 0)
                    {
                        strParams = strRequest.substr(start, i - start + 1);
                        break;
                    }
                }
            }
        }
    }

    // Find "id"
    strId = "null";
    pos = strRequest.find("\"id\"");
    if (pos != string::npos)
    {
        pos = strRequest.find(':', pos + 4);
        if (pos != string::npos)
        {
            pos++;
            while (pos < strRequest.size() && (strRequest[pos] == ' ' || strRequest[pos] == '\t'))
                pos++;

            // Read until comma, closing brace, or end
            size_t start = pos;
            while (pos < strRequest.size() && strRequest[pos] != ',' && strRequest[pos] != '}')
                pos++;
            strId = strRequest.substr(start, pos - start);

            // Trim trailing whitespace
            while (!strId.empty() && (strId[strId.size()-1] == ' ' || strId[strId.size()-1] == '\t' ||
                                      strId[strId.size()-1] == '\r' || strId[strId.size()-1] == '\n'))
                strId.erase(strId.size()-1);
        }
    }

    return true;
}


///
/// Extract the Nth string parameter from a JSON params array like ["val1","val2"]
///
string GetParamString(const string& strParams, int nIndex)
{
    int nCurrent = 0;
    size_t pos = 0;

    // Skip opening bracket
    if (!strParams.empty() && strParams[0] == '[')
        pos = 1;

    while (pos < strParams.size())
    {
        // Skip whitespace
        while (pos < strParams.size() && (strParams[pos] == ' ' || strParams[pos] == ',' || strParams[pos] == '\t'))
            pos++;

        if (pos >= strParams.size() || strParams[pos] == ']')
            break;

        if (strParams[pos] == '"')
        {
            // Quoted string
            pos++;
            size_t start = pos;
            while (pos < strParams.size() && strParams[pos] != '"')
            {
                if (strParams[pos] == '\\') pos++; // skip escaped char
                pos++;
            }
            if (nCurrent == nIndex)
                return strParams.substr(start, pos - start);
            pos++; // skip closing quote
        }
        else
        {
            // Unquoted value (number, bool, null)
            size_t start = pos;
            while (pos < strParams.size() && strParams[pos] != ',' && strParams[pos] != ']')
                pos++;
            if (nCurrent == nIndex)
            {
                string val = strParams.substr(start, pos - start);
                // Trim whitespace
                while (!val.empty() && (val[val.size()-1] == ' ' || val[val.size()-1] == '\t'))
                    val.erase(val.size()-1);
                return val;
            }
        }
        nCurrent++;
    }

    return "";
}


///
/// RPC command handlers
///

string HandleGetInfo()
{
    int nConnections = 0;
    CRITICAL_BLOCK(cs_vNodes)
        nConnections = vNodes.size();

    int64 nBalance = GetBalance();

    int nBgHeight = 0;
    CRITICAL_BLOCK(cs_bgold)
        nBgHeight = nBgoldHeight;

    string str;
    str += "{";
    str += "\"version\":" + JSONValue(VERSION) + ",";
    str += "\"balance\":" + JSONValue(FormatMoney(nBalance)) + ",";
    str += "\"blocks\":" + JSONValue(nBestHeight) + ",";
    str += "\"connections\":" + JSONValue(nConnections) + ",";
    str += "\"bgoldheight\":" + JSONValue(nBgHeight);
    str += "}";
    return str;
}

string HandleGetBalance()
{
    return JSONValue(FormatMoney(GetBalance()));
}

string HandleGetBlockCount()
{
    return JSONValue(nBestHeight);
}

string HandleGetNewAddress()
{
    // Generate a new key and return the base58 address
    vector<unsigned char> vchPubKey = GenerateNewKey();
    string strAddress = Hash160ToAddress(Hash160(vchPubKey));
    return JSONValue(strAddress);
}

string HandleSendToAddress(const string& strParams)
{
    string strAddress = GetParamString(strParams, 0);
    string strAmount = GetParamString(strParams, 1);

    if (strAddress.empty())
        return JSONError("Missing address parameter", "null");
    if (strAmount.empty())
        return JSONError("Missing amount parameter", "null");

    // Validate address
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        return JSONError("Invalid BC address", "null");

    // Parse amount
    int64 nAmount;
    if (!ParseMoney(strAmount.c_str(), nAmount))
        return JSONError("Invalid amount", "null");
    if (nAmount <= 0)
        return JSONError("Amount must be positive", "null");

    // Check balance
    if (nAmount > GetBalance())
        return JSONError("Insufficient funds", "null");

    // Build the script and send
    CScript scriptPubKey;
    scriptPubKey << OP_DUP << OP_HASH160 << hash160 << OP_EQUALVERIFY << OP_CHECKSIG;

    CWalletTx wtx;
    if (!SendMoney(scriptPubKey, nAmount, wtx))
        return JSONError("Send failed", "null");

    return JSONValue(wtx.GetHash().ToString());
}

string HandleListProducts()
{
    string str = "[";
    bool fFirst = true;

    CRITICAL_BLOCK(cs_mapProducts)
    {
        for (map<uint256, CProduct>::iterator mi = mapProducts.begin(); mi != mapProducts.end(); ++mi)
        {
            CProduct& product = (*mi).second;

            if (!fFirst) str += ",";
            fFirst = false;

            str += "{";
            str += "\"hash\":" + JSONValue((*mi).first.ToString()) + ",";

            // Extract title and description from mapValue if available
            string strTitle;
            string strCategory;
            string strPrice;
            map<string, string>::iterator it;

            it = product.mapValue.find("title");
            if (it != product.mapValue.end())
                strTitle = (*it).second;

            it = product.mapValue.find("category");
            if (it != product.mapValue.end())
                strCategory = (*it).second;

            it = product.mapValue.find("price");
            if (it != product.mapValue.end())
                strPrice = (*it).second;

            str += "\"title\":" + JSONValue(strTitle) + ",";
            str += "\"category\":" + JSONValue(strCategory) + ",";
            str += "\"price\":" + JSONValue(strPrice) + ",";
            str += "\"seller\":" + JSONValue(Hash160ToAddress(Hash160(product.vchPubKeyFrom)));
            str += "}";
        }
    }

    str += "]";
    return str;
}

string HandleListNews()
{
    vector<CNewsItem> vNews = GetTopNews(20);

    string str = "[";
    bool fFirst = true;

    for (int i = 0; i < vNews.size(); i++)
    {
        CNewsItem& item = vNews[i];

        if (!fFirst) str += ",";
        fFirst = false;

        str += "{";
        str += "\"hash\":" + JSONValue(item.GetHash().ToString()) + ",";
        str += "\"title\":" + JSONValue(item.strTitle) + ",";
        str += "\"url\":" + JSONValue(item.strURL) + ",";
        str += "\"text\":" + JSONValue(item.strText) + ",";
        str += "\"time\":" + JSONValue(item.nTime) + ",";
        str += "\"votes\":" + JSONValue(item.nVotes) + ",";
        str += "\"score\":" + JSONValue(GetNewsScore(item.nVotes, item.nTime)) + ",";
        str += "\"author\":" + JSONValue(Hash160ToAddress(Hash160(item.vchPubKey)));
        str += "}";
    }

    str += "]";
    return str;
}

string HandleSubmitNews(const string& strParams)
{
    string strTitle = GetParamString(strParams, 0);
    string strURL   = GetParamString(strParams, 1);
    string strText  = GetParamString(strParams, 2);

    if (strTitle.empty())
        return JSONError("Missing title parameter", "null");
    if (strURL.empty() && strText.empty())
        return JSONError("Must provide URL or text", "null");

    // Build the news item
    CNewsItem item;
    item.nVersion = 1;
    item.strTitle = strTitle;
    item.strURL = strURL;
    item.strText = strText;
    item.nTime = GetAdjustedTime();

    // Sign with wallet key
    CRITICAL_BLOCK(cs_mapKeys)
    {
        item.vchPubKey = keyUser.GetPubKey();
    }

    CKey key;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        vector<unsigned char> vchPubKey = keyUser.GetPubKey();
        map<vector<unsigned char>, CPrivKey>::iterator mi = mapKeys.find(vchPubKey);
        if (mi == mapKeys.end())
            return JSONError("No wallet key available for signing", "null");
        if (!key.SetPrivKey((*mi).second))
            return JSONError("Failed to set private key", "null");
    }

    if (!item.Sign(key))
        return JSONError("Failed to sign news item", "null");

    // Accept locally
    if (!AcceptNewsItem(item))
        return JSONError("News item not accepted", "null");

    // Relay to network
    uint256 hash = item.GetHash();
    RelayMessage(CInv(MSG_NEWS, hash), item);

    return JSONValue(hash.ToString());
}

string HandleVoteNews(const string& strParams)
{
    string strNewsHash = GetParamString(strParams, 0);
    string strUpvote   = GetParamString(strParams, 1);

    if (strNewsHash.empty())
        return JSONError("Missing news item hash parameter", "null");

    // Parse hash
    uint256 hashNews;
    hashNews.SetHex(strNewsHash);

    // Build the vote
    CNewsVote vote;
    vote.nVersion = 1;
    vote.hashNewsItem = hashNews;
    vote.fUpvote = (strUpvote.empty() || strUpvote == "true" || strUpvote == "1");
    vote.nTime = GetAdjustedTime();

    // Sign with wallet key
    CRITICAL_BLOCK(cs_mapKeys)
    {
        vote.vchPubKey = keyUser.GetPubKey();
    }

    CKey key;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        vector<unsigned char> vchPubKey = keyUser.GetPubKey();
        map<vector<unsigned char>, CPrivKey>::iterator mi = mapKeys.find(vchPubKey);
        if (mi == mapKeys.end())
            return JSONError("No wallet key available for signing", "null");
        if (!key.SetPrivKey((*mi).second))
            return JSONError("Failed to set private key", "null");
    }

    if (!vote.Sign(key))
        return JSONError("Failed to sign vote", "null");

    // Accept locally
    if (!AcceptNewsVote(vote))
        return JSONError("Vote not accepted", "null");

    // Relay to network
    uint256 hash = vote.GetHash();
    RelayMessage(CInv(MSG_NEWSVOTE, hash), vote);

    return JSONBool(true);
}

string HandleGetBgoldBalance()
{
    int64 nBgoldBal = 0;

    CRITICAL_BLOCK(cs_mapKeys)
    {
        vector<unsigned char> vchPubKey = keyUser.GetPubKey();
        nBgoldBal = GetBgoldBalance(vchPubKey);
    }

    // Return in display units (divide by COIN)
    return JSONValue(FormatMoney(nBgoldBal));
}


///
/// Block explorer RPC handlers
///

string HandleGetBlockHash(const string& strParams)
{
    string strHeight = GetParamString(strParams, 0);
    if (strHeight.empty())
        return JSONError("Missing height parameter", "null");

    int nHeight = atoi(strHeight.c_str());
    if (nHeight < 0 || nHeight > nBestHeight)
        return JSONError("Block height out of range", "null");

    CRITICAL_BLOCK(cs_main)
    {
        CBlockIndex* pindex = pindexGenesisBlock;
        for (int i = 0; i < nHeight && pindex; i++)
            pindex = pindex->pnext;

        if (!pindex)
            return JSONError("Block not found at height", "null");

        return JSONValue(pindex->GetBlockHash().ToString());
    }
}

string HandleGetBlock(const string& strParams)
{
    string strHash = GetParamString(strParams, 0);
    if (strHash.empty())
        return JSONError("Missing block hash parameter", "null");

    uint256 hash;
    hash.SetHex(strHash);

    CRITICAL_BLOCK(cs_main)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
        if (mi == mapBlockIndex.end())
            return JSONError("Block not found", "null");

        CBlockIndex* pindex = (*mi).second;

        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos, true))
            return JSONError("Failed to read block from disk", "null");

        string str;
        str += "{";
        str += "\"hash\":" + JSONValue(hash.ToString()) + ",";
        str += "\"version\":" + JSONValue(pindex->nVersion) + ",";
        str += "\"previousblockhash\":" + JSONValue(block.hashPrevBlock.ToString()) + ",";
        str += "\"merkleroot\":" + JSONValue(block.hashMerkleRoot.ToString()) + ",";
        str += "\"time\":" + JSONValue((int64)pindex->nTime) + ",";
        str += "\"bits\":" + JSONValue((int)pindex->nBits) + ",";
        str += "\"nonce\":" + JSONValue((int64)pindex->nNonce) + ",";
        str += "\"height\":" + JSONValue(pindex->nHeight) + ",";
        str += "\"txcount\":" + JSONValue((int)block.vtx.size()) + ",";

        // Next block hash (if exists)
        if (pindex->pnext)
            str += "\"nextblockhash\":" + JSONValue(pindex->pnext->GetBlockHash().ToString()) + ",";
        else
            str += "\"nextblockhash\":null,";

        // Transaction list
        str += "\"tx\":[";
        for (int i = 0; i < (int)block.vtx.size(); i++)
        {
            if (i > 0) str += ",";
            str += "{";
            str += "\"txid\":" + JSONValue(block.vtx[i].GetHash().ToString()) + ",";
            str += "\"coinbase\":" + JSONBool(block.vtx[i].IsCoinBase());
            if (block.vtx[i].IsCoinBase())
            {
                int64 nValue = 0;
                for (int j = 0; j < (int)block.vtx[i].vout.size(); j++)
                    nValue += block.vtx[i].vout[j].nValue;
                str += ",\"value\":" + JSONValue(FormatMoney(nValue));
            }
            str += "}";
        }
        str += "]";

        str += "}";
        return str;
    }
}

string HandleGetRawTransaction(const string& strParams)
{
    string strTxid = GetParamString(strParams, 0);
    if (strTxid.empty())
        return JSONError("Missing txid parameter", "null");

    uint256 hash;
    hash.SetHex(strTxid);

    CTxDB txdb("r");
    CTransaction tx;
    CTxIndex txindex;

    if (!txdb.ReadDiskTx(hash, tx, txindex))
        return JSONError("Transaction not found", "null");

    string str;
    str += "{";
    str += "\"txid\":" + JSONValue(hash.ToString()) + ",";
    str += "\"version\":" + JSONValue(tx.nVersion) + ",";
    str += "\"locktime\":" + JSONValue(tx.nLockTime) + ",";
    str += "\"coinbase\":" + JSONBool(tx.IsCoinBase()) + ",";

    // Find containing block
    CRITICAL_BLOCK(cs_main)
    {
        string strBlockHash;
        int nBlockHeight = -1;
        for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            CBlockIndex* pindex = (*mi).second;
            if (pindex->nFile == txindex.pos.nFile && pindex->nBlockPos == txindex.pos.nBlockPos)
            {
                strBlockHash = (*mi).first.ToString();
                nBlockHeight = pindex->nHeight;
                break;
            }
        }
        str += "\"blockhash\":" + JSONValue(strBlockHash) + ",";
        str += "\"blockheight\":" + JSONValue(nBlockHeight) + ",";
    }

    // Inputs
    str += "\"vin\":[";
    for (int i = 0; i < (int)tx.vin.size(); i++)
    {
        if (i > 0) str += ",";
        str += "{";
        if (tx.IsCoinBase())
        {
            str += "\"coinbase\":true";
        }
        else
        {
            str += "\"txid\":" + JSONValue(tx.vin[i].prevout.hash.ToString()) + ",";
            str += "\"vout\":" + JSONValue((int)tx.vin[i].prevout.n);
        }
        str += "}";
    }
    str += "],";

    // Outputs
    str += "\"vout\":[";
    for (int i = 0; i < (int)tx.vout.size(); i++)
    {
        if (i > 0) str += ",";
        str += "{";
        str += "\"value\":" + JSONValue(FormatMoney(tx.vout[i].nValue)) + ",";
        str += "\"n\":" + JSONValue(i) + ",";

        // Extract address
        uint160 hash160;
        if (ExtractHash160(tx.vout[i].scriptPubKey, hash160))
            str += "\"address\":" + JSONValue(Hash160ToAddress(hash160));
        else
            str += "\"address\":null";

        str += "}";
    }
    str += "]";

    str += "}";
    return str;
}

string HandleGetBlockchainInfo()
{
    CRITICAL_BLOCK(cs_main)
    {
        string str;
        str += "{";
        str += "\"blocks\":" + JSONValue(nBestHeight) + ",";
        str += "\"bestblockhash\":" + JSONValue(hashBestChain.ToString()) + ",";

        // Difficulty from nBits
        int nShift = (pindexBest->nBits >> 24) & 0xff;
        double dDiff = (double)0x0000ffff / (double)(pindexBest->nBits & 0x00ffffff);
        while (nShift < 29)
        {
            dDiff *= 256.0;
            nShift++;
        }
        while (nShift > 29)
        {
            dDiff /= 256.0;
            nShift--;
        }
        str += "\"difficulty\":" + JSONValue(dDiff) + ",";

        str += "\"genesishash\":" + JSONValue(hashGenesisBlock.ToString()) + ",";

        int nMempoolSize = 0;
        CRITICAL_BLOCK(cs_mapTransactions)
            nMempoolSize = mapTransactions.size();
        str += "\"mempoolsize\":" + JSONValue(nMempoolSize);

        str += "}";
        return str;
    }
}

string HandleGetRecentBlocks()
{
    CRITICAL_BLOCK(cs_main)
    {
        string str = "[";
        CBlockIndex* pindex = pindexBest;
        bool fFirst = true;
        int nCount = 0;
        while (pindex && nCount < 20)
        {
            if (!fFirst) str += ",";
            fFirst = false;

            CBlock block;
            block.ReadFromDisk(pindex->nFile, pindex->nBlockPos, true);

            // Calculate time since previous block
            int64 nTimeDelta = 0;
            if (pindex->pprev)
                nTimeDelta = pindex->nTime - pindex->pprev->nTime;

            str += "{";
            str += "\"height\":" + JSONValue(pindex->nHeight) + ",";
            str += "\"hash\":" + JSONValue(pindex->GetBlockHash().ToString()) + ",";
            str += "\"time\":" + JSONValue((int64)pindex->nTime) + ",";
            str += "\"timedelta\":" + JSONValue(nTimeDelta) + ",";
            str += "\"txcount\":" + JSONValue((int)block.vtx.size()) + ",";
            str += "\"nonce\":" + JSONValue((int64)pindex->nNonce) + ",";
            str += "\"bits\":" + JSONValue((int)pindex->nBits);
            str += "}";

            pindex = pindex->pprev;
            nCount++;
        }
        str += "]";
        return str;
    }
}

string HandleGetRawMempool()
{
    string str = "[";
    bool fFirst = true;

    CRITICAL_BLOCK(cs_mapTransactions)
    {
        for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi)
        {
            if (!fFirst) str += ",";
            fFirst = false;
            str += JSONValue((*mi).first.ToString());
        }
    }

    str += "]";
    return str;
}


string HandleListTransactions()
{
    string str = "[";
    bool fFirst = true;

    vector<pair<int64, const CWalletTx*> > vSorted;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
            vSorted.push_back(make_pair(it->second.GetTxTime(), &it->second));
    }
    sort(vSorted.begin(), vSorted.end(),
         [](const pair<int64, const CWalletTx*>& a, const pair<int64, const CWalletTx*>& b) {
             return a.first > b.first;
         });

    int nCount = 0;
    for (auto& item : vSorted)
    {
        if (nCount++ >= 100) break;
        const CWalletTx* pwtx = item.second;
        int64 nCredit = pwtx->GetCredit();
        int64 nDebit = pwtx->GetDebit();
        int64 nNet = nCredit - nDebit;
        int nConf = pwtx->GetDepthInMainChain();

        if (!fFirst) str += ",";
        fFirst = false;

        str += "{";
        str += "\"txid\":" + JSONValue(pwtx->GetHash().ToString()) + ",";
        str += "\"time\":" + JSONValue(pwtx->GetTxTime()) + ",";
        str += "\"amount\":" + JSONValue(FormatMoney(nNet)) + ",";
        str += "\"confirmations\":" + JSONValue(nConf) + ",";
        str += "\"coinbase\":" + JSONBool(pwtx->IsCoinBase());

        // Extract address
        if (pwtx->IsCoinBase())
        {
            str += ",\"type\":\"generate\"";
            for (int i = 0; i < (int)pwtx->vout.size(); i++)
            {
                if (pwtx->vout[i].nValue > 0)
                {
                    uint160 hash160;
                    if (ExtractHash160(pwtx->vout[i].scriptPubKey, hash160))
                        str += ",\"address\":" + JSONValue(Hash160ToAddress(hash160));
                    break;
                }
            }
        }
        else if (nNet > 0)
        {
            str += ",\"type\":\"receive\"";
            for (int i = 0; i < (int)pwtx->vout.size(); i++)
            {
                if (pwtx->vout[i].IsMine())
                {
                    uint160 hash160;
                    if (ExtractHash160(pwtx->vout[i].scriptPubKey, hash160))
                        str += ",\"address\":" + JSONValue(Hash160ToAddress(hash160));
                    break;
                }
            }
        }
        else
        {
            str += ",\"type\":\"send\"";
            for (int i = 0; i < (int)pwtx->vout.size(); i++)
            {
                if (!pwtx->vout[i].IsMine())
                {
                    uint160 hash160;
                    if (ExtractHash160(pwtx->vout[i].scriptPubKey, hash160))
                        str += ",\"address\":" + JSONValue(Hash160ToAddress(hash160));
                    break;
                }
            }
        }

        str += "}";
    }

    str += "]";
    return str;
}

string HandleListPeers()
{
    string str = "[";
    bool fFirst = true;

    CRITICAL_BLOCK(cs_vNodes)
    {
        foreach(CNode* pnode, vNodes)
        {
            if (!fFirst) str += ",";
            fFirst = false;
            str += "{";
            str += "\"addr\":" + JSONValue(pnode->addr.ToStringIPPort()) + ",";
            str += "\"version\":" + JSONValue(pnode->nVersion) + ",";
            str += "\"inbound\":" + JSONBool(pnode->fInbound);
            str += "}";
        }
    }

    str += "]";
    return str;
}

string HandleGetMiningInfo()
{
    string str = "{";
    str += "\"blocks\":" + JSONValue(nBestHeight) + ",";
    str += "\"balance\":" + JSONValue(FormatMoney(GetBalance())) + ",";
    str += "\"generate\":" + JSONBool(fGenerateBcash) + ",";
    str += "\"connections\":" + JSONValue((int)vNodes.size()) + ",";

    // Current difficulty
    double dDiff = 1.0;
    if (pindexBest)
    {
        int nShift = (pindexBest->nBits >> 24) & 0xff;
        double dTarget = (double)(pindexBest->nBits & 0x00ffffff);
        while (nShift < 29) { dTarget *= 256.0; nShift++; }
        while (nShift > 29) { dTarget /= 256.0; nShift--; }
        dDiff = (double)0x00ffff / dTarget;
    }
    str += "\"difficulty\":" + JSONValue(dDiff) + ",";

    // Hashrate estimate: blocks in last hour * difficulty * 2^32 / 3600
    int nBlocksLastHour = 0;
    if (pindexBest)
    {
        CBlockIndex* pindex = pindexBest;
        int64 nNow = GetTime();
        while (pindex && (nNow - pindex->nTime) < 3600)
        {
            nBlocksLastHour++;
            pindex = pindex->pprev;
        }
    }
    double dHashrate = nBlocksLastHour * dDiff * 4294967296.0 / 3600.0;
    str += "\"hashrate\":" + JSONValue(dHashrate) + ",";

    // Thread count
    str += "\"threads\":" + JSONValue(nMiningThreads);

    str += "}";
    return str;
}

string HandleSetGenerate(const string& strParams)
{
    string strGen = GetParamString(strParams, 0);
    if (strGen == "true" || strGen == "1")
    {
        fGenerateBcash = true;
        StartMultiMiner();
        return JSONBool(true);
    }
    else
    {
        fGenerateBcash = false;
        return JSONBool(false);
    }
}

string HandleAddNode(const string& strParams)
{
    string strAddr = GetParamString(strParams, 0);
    if (strAddr.empty())
        return JSONError("Missing address parameter (host:port)", "null");

    // Parse host:port
    string strHost = strAddr;
    unsigned short nPort = ntohs(DEFAULT_PORT);
    size_t colonPos = strAddr.rfind(':');
    if (colonPos != string::npos)
    {
        nPort = atoi(strAddr.substr(colonPos + 1).c_str());
        strHost = strAddr.substr(0, colonPos);
    }

    // Resolve hostname
    struct hostent* phostent = gethostbyname(strHost.c_str());
    if (!phostent || !phostent->h_addr_list[0])
        return JSONError("Could not resolve hostname: " + strHost, "null");

    CAddress addr(*(unsigned int*)phostent->h_addr_list[0], htons(nPort));
    printf("RPC addnode: connecting to %s\n", addr.ToString().c_str());

    CNode* pnode = ConnectNode(addr);
    if (pnode)
    {
        pnode->fNetworkNode = true;
        return JSONValue("Connected to " + addr.ToString());
    }
    return JSONError("Failed to connect to " + strAddr, "null");
}

///
/// Bitboard RPC handlers
///

string HandleListThreads(const string& strParams)
{
    string strBoard = GetParamString(strParams, 0);
    if (strBoard.empty())
        strBoard = "/b/";

    string str = "[";
    bool fFirst = true;

    CRITICAL_BLOCK(cs_imageboard)
    {
        if (mapBoardThreads.count(strBoard))
        {
            vector<uint256>& vThreads = mapBoardThreads[strBoard];
            // Iterate in reverse (newest first)
            for (int i = (int)vThreads.size() - 1; i >= 0; i--)
            {
                if (mapImagePosts.count(vThreads[i]))
                {
                    CImagePost& op = mapImagePosts[vThreads[i]];
                    if (!fFirst) str += ",";
                    fFirst = false;

                    int nReplies = 0;
                    if (mapThreadReplies.count(vThreads[i]))
                        nReplies = (int)mapThreadReplies[vThreads[i]].size();

                    str += "{";
                    str += "\"hash\":" + JSONValue(vThreads[i].ToString()) + ",";
                    str += "\"subject\":" + JSONValue(op.strSubject) + ",";
                    str += "\"comment\":" + JSONValue(op.strComment) + ",";
                    str += "\"time\":" + JSONValue((int64)op.nTime) + ",";
                    str += "\"tripcode\":" + JSONValue(op.GetTripcode()) + ",";
                    str += "\"replies\":" + JSONValue(nReplies) + ",";
                    str += "\"hasimage\":" + JSONBool(!op.vchImage.empty());
                    str += "}";
                }
            }
        }
    }

    str += "]";
    return str;
}

string HandleGetThread(const string& strParams)
{
    string strHash = GetParamString(strParams, 0);
    if (strHash.empty())
        return JSONError("Missing thread hash", "null");

    uint256 hash;
    hash.SetHex(strHash);

    string str = "{";

    CRITICAL_BLOCK(cs_imageboard)
    {
        if (!mapImagePosts.count(hash))
            return JSONError("Thread not found", "null");

        CImagePost& op = mapImagePosts[hash];
        str += "\"op\":{";
        str += "\"hash\":" + JSONValue(hash.ToString()) + ",";
        str += "\"subject\":" + JSONValue(op.strSubject) + ",";
        str += "\"comment\":" + JSONValue(op.strComment) + ",";
        str += "\"time\":" + JSONValue((int64)op.nTime) + ",";
        str += "\"tripcode\":" + JSONValue(op.GetTripcode()) + ",";
        str += "\"hasimage\":" + JSONBool(!op.vchImage.empty());
        str += "},";

        str += "\"replies\":[";
        bool fFirst = true;
        if (mapThreadReplies.count(hash))
        {
            foreach(const uint256& replyHash, mapThreadReplies[hash])
            {
                if (mapImagePosts.count(replyHash))
                {
                    CImagePost& reply = mapImagePosts[replyHash];
                    if (!fFirst) str += ",";
                    fFirst = false;
                    str += "{";
                    str += "\"hash\":" + JSONValue(replyHash.ToString()) + ",";
                    str += "\"comment\":" + JSONValue(reply.strComment) + ",";
                    str += "\"time\":" + JSONValue((int64)reply.nTime) + ",";
                    str += "\"tripcode\":" + JSONValue(reply.GetTripcode()) + ",";
                    str += "\"hasimage\":" + JSONBool(!reply.vchImage.empty());
                    str += "}";
                }
            }
        }
        str += "]";
    }

    str += "}";
    return str;
}

// Simple base64 encode
static string Base64Encode(const vector<unsigned char>& data)
{
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string result;
    int val = 0, bits = 0;
    for (size_t i = 0; i < data.size(); i++)
    {
        val = (val << 8) + data[i];
        bits += 8;
        while (bits >= 6)
        {
            bits -= 6;
            result += b64[(val >> bits) & 0x3F];
        }
    }
    if (bits > 0)
        result += b64[(val << (6 - bits)) & 0x3F];
    while (result.size() % 4)
        result += '=';
    return result;
}

string HandleGetPostImage(const string& strParams)
{
    string strHash = GetParamString(strParams, 0);
    if (strHash.empty())
        return JSONError("Missing post hash", "null");

    uint256 hash;
    hash.SetHex(strHash);

    CRITICAL_BLOCK(cs_imageboard)
    {
        if (!mapImagePosts.count(hash))
            return JSONError("Post not found", "null");

        CImagePost& post = mapImagePosts[hash];
        if (post.vchImage.empty())
            return JSONError("Post has no image", "null");

        // Parse header: width (2 bytes LE) + height (2 bytes LE) + RLE data
        if (post.vchImage.size() < 5)
            return JSONError("Invalid image data", "null");

        int nWidth = post.vchImage[0] | (post.vchImage[1] << 8);
        int nHeight = post.vchImage[2] | (post.vchImage[3] << 8);

        // Decompress RLE to indexed pixels
        vector<unsigned char> vchRLE(post.vchImage.begin() + 4, post.vchImage.end());
        vector<unsigned char> vchIndexed = DecompressRLE(vchRLE);

        // Return as base64 indexed pixels with dimensions
        string str = "{";
        str += "\"width\":" + JSONValue(nWidth) + ",";
        str += "\"height\":" + JSONValue(nHeight) + ",";
        str += "\"pixels\":" + JSONValue(Base64Encode(vchIndexed));
        str += "}";
        return str;
    }
}

// Simple base64 decode
static vector<unsigned char> Base64Decode(const string& str)
{
    static const string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    vector<unsigned char> result;
    int val = 0, bits = -8;
    for (size_t i = 0; i < str.size(); i++)
    {
        size_t pos = b64.find(str[i]);
        if (pos == string::npos) continue;
        val = (val << 6) + (int)pos;
        bits += 6;
        if (bits >= 0)
        {
            result.push_back((unsigned char)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return result;
}

string HandleCreatePost(const string& strParams)
{
    string strBoard = GetParamString(strParams, 0);
    string strSubject = GetParamString(strParams, 1);
    string strComment = GetParamString(strParams, 2);
    string strThreadHash = GetParamString(strParams, 3);
    string strImageB64 = GetParamString(strParams, 4);
    string strWidth = GetParamString(strParams, 5);
    string strHeight = GetParamString(strParams, 6);

    if (strBoard.empty())
        return JSONError("Missing board parameter", "null");
    if (strComment.empty() && strImageB64.empty())
        return JSONError("Need comment or image", "null");

    uint256 hashThread = 0;
    if (!strThreadHash.empty())
        hashThread.SetHex(strThreadHash);

    vector<unsigned char> vchImage;
    string strMode = GetParamString(strParams, 7);
    if (!strImageB64.empty())
    {
        int nWidth = atoi(strWidth.c_str());
        int nHeight = atoi(strHeight.c_str());

        if (strMode == "indexed")
        {
            // Pre-dithered indexed pixels from client (1 byte per pixel, 0-15)
            vector<unsigned char> vchIndexed = Base64Decode(strImageB64);
            if (nWidth > 0 && nHeight > 0 && (int)vchIndexed.size() >= nWidth * nHeight)
            {
                vector<unsigned char> vchCompressed = CompressRLE(vchIndexed);
                vchImage.push_back((unsigned char)(nWidth & 0xFF));
                vchImage.push_back((unsigned char)((nWidth >> 8) & 0xFF));
                vchImage.push_back((unsigned char)(nHeight & 0xFF));
                vchImage.push_back((unsigned char)((nHeight >> 8) & 0xFF));
                vchImage.insert(vchImage.end(), vchCompressed.begin(), vchCompressed.end());
                printf("CreatePost: pre-dithered image %dx%d, indexed %d bytes, RLE %d bytes\n",
                    nWidth, nHeight, (int)vchIndexed.size(), (int)vchImage.size());
            }
        }
        else
        {
            // Raw RGB data - dither on server
            vector<unsigned char> vchRaw = Base64Decode(strImageB64);
            if (nWidth > 0 && nHeight > 0 && (int)vchRaw.size() >= nWidth * nHeight * 3)
            {
                vector<unsigned char> vchDithered = DitherImage(vchRaw, nWidth, nHeight);
                vector<unsigned char> vchCompressed = CompressRLE(vchDithered);
                vchImage.push_back((unsigned char)(nWidth & 0xFF));
                vchImage.push_back((unsigned char)((nWidth >> 8) & 0xFF));
                vchImage.push_back((unsigned char)(nHeight & 0xFF));
                vchImage.push_back((unsigned char)((nHeight >> 8) & 0xFF));
                vchImage.insert(vchImage.end(), vchCompressed.begin(), vchCompressed.end());
                printf("CreatePost: image %dx%d, raw %d bytes, dithered+RLE %d bytes\n",
                    nWidth, nHeight, (int)vchRaw.size(), (int)vchImage.size());
            }
        }
    }

    if (!CreateImagePost(strBoard, strSubject, strComment, vchImage, hashThread))
        return JSONError("Insufficient funds — posts require on-chain transaction (mine blocks first)", "null");

    return JSONValue("Post created");
}


///
/// Web UI HTML - served on GET /
///
string GetWebUIHTML()
{
    string html;
    html += "<!DOCTYPE html><html lang='en'><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<title>bnet</title>";
    html += "<link href='https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@300;400;500;700&display=swap' rel='stylesheet'>";
    html += "<style>";

    // ─── CSS ───
    html += R"CSS(
*{margin:0;padding:0;box-sizing:border-box}
:root{--bg:#0a0a0a;--bg2:#111;--bg3:#1a1a1a;--border:#222;--green:#00ff41;--green2:#00cc33;--dim:#555;--text:#ccc;--white:#eee}
body{background:var(--bg);color:var(--text);font-family:'JetBrains Mono',monospace;font-size:13px;display:flex;height:100vh;overflow:hidden}
::selection{background:var(--green);color:#000}
::-webkit-scrollbar{width:6px}::-webkit-scrollbar-track{background:var(--bg2)}::-webkit-scrollbar-thumb{background:var(--border);border-radius:3px}

/* Sidebar */
.sidebar{width:200px;min-width:200px;background:var(--bg2);border-right:1px solid var(--border);display:flex;flex-direction:column;padding:0}
.logo{padding:16px;font-size:18px;font-weight:700;color:var(--green);border-bottom:1px solid var(--border);letter-spacing:2px;text-align:center}
.logo span{color:var(--dim);font-weight:300}
.nav{flex:1;padding:8px 0}
.nav-item{display:flex;align-items:center;padding:10px 16px;cursor:pointer;color:var(--dim);transition:all .15s;border-left:2px solid transparent;font-size:12px}
.nav-item:hover{color:var(--text);background:var(--bg3)}
.nav-item.active{color:var(--green);border-left-color:var(--green);background:var(--bg3)}
.nav-item .icon{width:24px;text-align:center;margin-right:10px;font-size:14px}
.sidebar-footer{padding:12px 16px;border-top:1px solid var(--border);font-size:10px;color:var(--dim)}

/* Main */
.main{flex:1;display:flex;flex-direction:column;overflow:hidden}
.topbar{height:40px;background:var(--bg2);border-bottom:1px solid var(--border);display:flex;align-items:center;padding:0 16px;font-size:11px;color:var(--dim)}
.topbar .status{margin-left:auto;display:flex;gap:16px}
.topbar .dot{width:6px;height:6px;border-radius:50%;background:var(--green);display:inline-block;margin-right:4px;animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.content{flex:1;overflow-y:auto;padding:20px}
.tab{display:none}.tab.active{display:block}

/* Cards */
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin-bottom:20px}
.card{background:var(--bg2);border:1px solid var(--border);border-radius:4px;padding:16px}
.card-label{font-size:10px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);margin-bottom:4px}
.card-value{font-size:24px;font-weight:700;color:var(--green)}
.card-sub{font-size:10px;color:var(--dim);margin-top:4px}

/* Tables */
table{width:100%;border-collapse:collapse}
th{text-align:left;padding:8px 12px;font-size:10px;text-transform:uppercase;letter-spacing:1px;color:var(--dim);border-bottom:1px solid var(--border);font-weight:400}
td{padding:8px 12px;border-bottom:1px solid var(--border);font-size:12px}
tr:hover td{background:var(--bg3)}
.amount-pos{color:var(--green)}.amount-neg{color:#ff4444}

/* Section */
.section{margin-bottom:24px}
.section-title{font-size:11px;text-transform:uppercase;letter-spacing:2px;color:var(--dim);margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid var(--border)}

/* Forms */
input,textarea,select{background:var(--bg);border:1px solid var(--border);color:var(--text);padding:8px 12px;font-family:inherit;font-size:12px;border-radius:3px;outline:none;width:100%}
input:focus,textarea:focus{border-color:var(--green)}
.btn{background:var(--bg3);border:1px solid var(--green);color:var(--green);padding:8px 20px;cursor:pointer;font-family:inherit;font-size:12px;border-radius:3px;transition:all .15s}
.btn:hover{background:var(--green);color:#000}
.btn-sm{padding:4px 12px;font-size:11px}
.form-row{display:flex;gap:8px;margin-bottom:8px;align-items:center}
.form-row label{min-width:80px;font-size:11px;color:var(--dim)}

/* Chess */
.chess-board{display:grid;grid-template-columns:repeat(8,48px);grid-template-rows:repeat(8,48px);border:2px solid var(--border);margin:12px 0}
.chess-sq{display:flex;align-items:center;justify-content:center;font-size:32px;cursor:pointer}
.chess-sq.light{background:#1a2a1a}.chess-sq.dark{background:#0d150d}
.chess-sq:hover{outline:2px solid var(--green);outline-offset:-2px}
.chess-sq.selected{outline:2px solid var(--green)}

/* Poker */
.poker-table{background:#0a1a0a;border:2px solid #1a3a1a;border-radius:120px;padding:40px;text-align:center;margin:12px 0;min-height:200px;display:flex;align-items:center;justify-content:center;gap:8px}
.playing-card{width:60px;height:90px;background:#fff;color:#000;border-radius:6px;display:inline-flex;flex-direction:column;align-items:center;justify-content:center;font-size:18px;font-weight:700;margin:0 2px;box-shadow:0 2px 8px rgba(0,0,0,.5)}
.playing-card.red{color:#cc0000}
.playing-card.facedown{background:var(--bg3);color:transparent;border:2px solid var(--green);background-image:repeating-linear-gradient(45deg,transparent,transparent 5px,rgba(0,255,65,.05) 5px,rgba(0,255,65,.05) 10px)}

/* Bitboard */
.thread{background:var(--bg2);border:1px solid var(--border);padding:12px;margin-bottom:8px;border-radius:3px}
.thread-header{display:flex;gap:8px;font-size:11px;margin-bottom:4px}
.tripcode{color:var(--green)}
.thread-subject{font-weight:700;color:var(--white)}
.board-tabs{display:flex;gap:4px;margin-bottom:12px}
.board-tab{padding:4px 12px;border:1px solid var(--border);cursor:pointer;border-radius:3px;font-size:11px}
.board-tab.active{border-color:var(--green);color:var(--green)}

/* News */
.news-item{display:flex;align-items:start;padding:8px 0;border-bottom:1px solid var(--border)}
.news-vote{display:flex;flex-direction:column;align-items:center;min-width:40px;color:var(--dim);cursor:pointer;font-size:10px}
.news-vote .arrow{font-size:16px;line-height:1;color:var(--dim);cursor:pointer}
.news-vote .arrow:hover{color:var(--green)}
.news-content{flex:1;margin-left:8px}
.news-title{color:var(--white);font-size:13px}
.news-title a{color:inherit;text-decoration:none}.news-title a:hover{color:var(--green)}
.news-meta{font-size:10px;color:var(--dim);margin-top:2px}

/* Market */
.product-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:12px}
.product-card{background:var(--bg2);border:1px solid var(--border);border-radius:4px;padding:16px;cursor:pointer;transition:border-color .15s}
.product-card:hover{border-color:var(--green)}
.product-title{font-weight:500;color:var(--white);margin-bottom:4px}
.product-price{color:var(--green);font-size:16px;font-weight:700}
.product-seller{font-size:10px;color:var(--dim);margin-top:4px}

/* Loading */
.loading{display:inline-block;width:16px;height:16px;border:2px solid var(--border);border-top-color:var(--green);border-radius:50%;animation:spin .6s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

/* Block mini chart */
.block-chart{display:flex;align-items:flex-end;gap:2px;height:60px;margin-top:8px}
.block-bar{width:6px;background:var(--green);border-radius:1px;min-height:2px;opacity:.6;transition:height .3s}
)CSS";

    html += "</style></head><body>";

    // ─── Sidebar ───
    html += R"HTML(
<div class="sidebar">
  <div class="logo">B<span>NET</span></div>
  <div class="nav">
    <div class="nav-item active" onclick="showTab('dashboard')"><span class="icon">&#9632;</span>Dashboard</div>
    <div class="nav-item" onclick="showTab('wallet')"><span class="icon">&#9733;</span>Wallet</div>
    <div class="nav-item" onclick="showTab('explorer')"><span class="icon">&#9830;</span>Explorer</div>
    <div class="nav-item" onclick="showTab('chess')"><span class="icon">&#9822;</span>Chess</div>
    <div class="nav-item" onclick="showTab('poker')"><span class="icon">&#9824;</span>Poker</div>
    <div class="nav-item" onclick="showTab('imageboard')"><span class="icon">&#9998;</span>Bitboard</div>
    <div class="nav-item" onclick="showTab('news')"><span class="icon">&#9889;</span>News</div>
    <div class="nav-item" onclick="showTab('market')"><span class="icon">&#9878;</span>Market</div>
    <div class="nav-item" onclick="showTab('peers')"><span class="icon">&#8943;</span>Peers</div>
    <div class="nav-item" onclick="showTab('nodes')"><span class="icon">&#9881;</span>Nodes</div>
  </div>
  <div class="sidebar-footer">bnet v0.2.0<br><span id="clock"></span></div>
</div>
)HTML";

    // ─── Main ───
    html += R"HTML(
<div class="main">
  <div class="topbar">
    <span>&#9632; bnet node</span>
    <div class="status">
      <span><span class="dot"></span> <span id="tb-connections">0</span> peers</span>
      <span>Height: <span id="tb-height">0</span></span>
      <span id="tb-balance">0.00 BC</span>
    </div>
  </div>
  <div class="content">
)HTML";

    // ─── Dashboard Tab ───
    html += R"HTML(
    <div class="tab active" id="tab-dashboard">
      <div class="section-title">Dashboard</div>
      <div class="cards">
        <div class="card"><div class="card-label">Balance</div><div class="card-value" id="dash-balance">0.00</div><div class="card-sub">BC</div></div>
        <div class="card"><div class="card-label">Block Height</div><div class="card-value" id="dash-height">0</div><div class="card-sub">blocks</div></div>
        <div class="card"><div class="card-label">Connections</div><div class="card-value" id="dash-connections">0</div><div class="card-sub">peers</div></div>
        <div class="card"><div class="card-label">Difficulty</div><div class="card-value" id="dash-difficulty">0</div><div class="card-sub">target</div></div>
        <div class="card"><div class="card-label">Hashrate</div><div class="card-value" id="dash-hashrate">0</div><div class="card-sub">H/s</div></div>
        <div class="card"><div class="card-label">Mining</div><div class="card-value" id="dash-mining">OFF</div><div class="card-sub" id="dash-mining-sub">idle</div></div>
      </div>
      <div class="section-title">Recent Blocks</div>
      <table><thead><tr><th>Height</th><th>Hash</th><th>Time</th><th>Delta</th><th>Txs</th><th>Nonce</th></tr></thead><tbody id="dash-blocks"></tbody></table>
      <div class="section-title" style="margin-top:20px">Recent Transactions</div>
      <table><thead><tr><th>Time</th><th>Type</th><th>Address</th><th>Amount</th><th>Conf</th></tr></thead><tbody id="dash-txlist"></tbody></table>
    </div>
)HTML";

    // ─── Wallet Tab ───
    html += R"HTML(
    <div class="tab" id="tab-wallet">
      <div class="section-title">Send BC</div>
      <div class="form-row"><label>To:</label><input id="send-addr" placeholder="Recipient address"></div>
      <div class="form-row"><label>Amount:</label><input id="send-amount" placeholder="0.00" style="width:200px"><button class="btn" onclick="doSend()" style="margin-left:8px">Send</button></div>
      <div id="send-result" style="margin:8px 0;font-size:11px"></div>
      <div class="section-title" style="margin-top:20px">Receive</div>
      <div class="form-row"><label>Address:</label><input id="recv-addr" readonly style="color:var(--green)"><button class="btn btn-sm" onclick="copyAddr()" style="margin-left:8px">Copy</button><button class="btn btn-sm" onclick="newAddr()" style="margin-left:4px">New</button></div>
      <div class="section-title" style="margin-top:20px">All Transactions</div>
      <table><thead><tr><th>Date</th><th>Type</th><th>Address</th><th>Amount</th><th>Confirmations</th></tr></thead><tbody id="wallet-txlist"></tbody></table>
    </div>
)HTML";

    // ─── Explorer Tab ───
    html += R"HTML(
    <div class="tab" id="tab-explorer">
      <div class="section-title">Block Explorer</div>
      <div class="form-row"><input id="explorer-search" placeholder="Block hash, height, or transaction ID"><button class="btn" onclick="doExplore()" style="margin-left:8px">Search</button></div>
      <div id="explorer-result" style="margin-top:16px"></div>
    </div>
)HTML";

    // ─── Chess Tab ───
    html += R"HTML(
    <div class="tab" id="tab-chess">
      <div class="section-title">Chess</div>
      <div style="display:flex;gap:20px">
        <div>
          <div class="chess-board" id="chess-board"></div>
          <div class="form-row" style="margin-top:8px"><input id="chess-move" placeholder="e2e4" style="width:100px"><button class="btn btn-sm" onclick="makeChessMove()" style="margin-left:8px">Move</button></div>
        </div>
        <div style="flex:1">
          <div class="section-title">Open Challenges</div>
          <div id="chess-challenges"><span style="color:var(--dim)">No open challenges</span></div>
          <button class="btn" style="margin-top:12px" onclick="challengeChess()">New Challenge</button>
          <div class="section-title" style="margin-top:16px">Move History</div>
          <div id="chess-history" style="color:var(--dim);font-size:11px"></div>
        </div>
      </div>
    </div>
)HTML";

    // ─── Poker Tab ───
    html += R"HTML(
    <div class="tab" id="tab-poker">
      <div class="section-title">5-Card Draw Poker</div>
      <div class="poker-table" id="poker-table">
        <div style="color:var(--dim)">No active game. Create or join a table.</div>
      </div>
      <div style="display:flex;gap:8px;justify-content:center;margin:12px 0">
        <button class="btn" onclick="pokerAction('fold')">Fold</button>
        <button class="btn" onclick="pokerAction('call')">Call</button>
        <button class="btn" onclick="pokerAction('raise')">Raise</button>
        <button class="btn" onclick="pokerAction('deal')">Deal</button>
      </div>
      <div style="text-align:center;color:var(--dim);font-size:11px">Pot: <span id="poker-pot" style="color:var(--green)">0.00</span> BC</div>
    </div>
)HTML";

    // ─── Bitboard Tab ───
    html += R"HTML(
    <div class="tab" id="tab-imageboard">
      <div style="text-align:center;margin-bottom:8px"><canvas id="ib-logo" style="image-rendering:pixelated" width="1" height="1"></canvas></div>
      <div style="color:var(--dim);font-size:10px;margin-bottom:12px;text-align:center">On-chain imageboard. All posts are broadcast as transactions. Tripcodes derived from your wallet key.</div>
      <div class="board-tabs">
        <div class="board-tab active" onclick="switchBoard('/b/')">/b/</div>
        <div class="board-tab" onclick="switchBoard('/g/')">/g/</div>
        <div class="board-tab" onclick="switchBoard('/biz/')">/biz/</div>
      </div>
      <div id="ib-thread-view" style="display:none;margin-bottom:12px">
        <div style="cursor:pointer;color:var(--green);font-size:11px;margin-bottom:8px" onclick="closeThread()">&larr; Back to board</div>
        <div id="ib-thread-content"></div>
        <div class="section" style="background:var(--bg2);padding:12px;border-radius:3px;margin-top:12px">
          <div class="form-row"><label>Reply:</label><textarea id="ib-reply" rows="2" placeholder="Your reply..."></textarea></div>
          <div class="form-row">
            <label>Image:</label><input type="file" id="ib-reply-image" accept="image/*" style="font-size:11px" onchange="onImageSelect(this,'reply')">
            <select id="ib-reply-dither" style="margin-left:8px;font-size:11px;background:var(--bg3);color:var(--text);border:1px solid var(--border);padding:2px 4px" onchange="updatePreview('reply')">
              <option value="floyd-steinberg">Floyd-Steinberg</option>
              <option value="ordered">Ordered (Bayer)</option>
              <option value="atkinson">Atkinson</option>
              <option value="none">No dithering</option>
            </select>
          </div>
          <div id="ib-reply-preview" style="display:none;margin:8px 0;padding:8px;background:var(--bg3);border-radius:3px">
            <div style="display:flex;gap:12px;align-items:flex-start">
              <div><div style="font-size:9px;color:var(--dim)">Original</div><canvas id="ib-reply-prev-orig" style="image-rendering:pixelated;border:1px solid var(--border)"></canvas></div>
              <div><div style="font-size:9px;color:var(--dim)">Dithered (16-color CGA)</div><canvas id="ib-reply-prev-dith" style="image-rendering:pixelated;border:1px solid var(--border)"></canvas></div>
            </div>
            <div style="font-size:9px;color:var(--dim);margin-top:4px" id="ib-reply-prev-info"></div>
          </div>
          <button class="btn btn-sm" onclick="postReply()">Reply</button>
          <span id="ib-reply-status" style="margin-left:8px;font-size:10px;color:var(--dim)"></span>
        </div>
      </div>
      <div id="ib-board-view">
        <div class="section" style="background:var(--bg2);padding:12px;border-radius:3px;margin-bottom:12px">
          <div class="form-row"><label>Subject:</label><input id="ib-subject" placeholder="Thread subject"></div>
          <div class="form-row"><label>Comment:</label><textarea id="ib-comment" rows="3" placeholder="Your message..."></textarea></div>
          <div class="form-row">
            <label>Image:</label><input type="file" id="ib-image" accept="image/*" style="font-size:11px" onchange="onImageSelect(this,'main')">
            <select id="ib-dither-algo" style="margin-left:8px;font-size:11px;background:var(--bg3);color:var(--text);border:1px solid var(--border);padding:2px 4px" onchange="updatePreview('main')">
              <option value="floyd-steinberg">Floyd-Steinberg</option>
              <option value="ordered">Ordered (Bayer)</option>
              <option value="atkinson">Atkinson</option>
              <option value="none">No dithering</option>
            </select>
          </div>
          <div id="ib-preview" style="display:none;margin:8px 0;padding:8px;background:var(--bg3);border-radius:3px">
            <div style="display:flex;gap:12px;align-items:flex-start">
              <div><div style="font-size:9px;color:var(--dim)">Original</div><canvas id="ib-prev-orig" style="image-rendering:pixelated;border:1px solid var(--border)"></canvas></div>
              <div><div style="font-size:9px;color:var(--dim)">Dithered (16-color CGA)</div><canvas id="ib-prev-dith" style="image-rendering:pixelated;border:1px solid var(--border)"></canvas></div>
            </div>
            <div style="font-size:9px;color:var(--dim);margin-top:4px" id="ib-prev-info"></div>
          </div>
          <button class="btn btn-sm" onclick="postThread()">New Thread</button>
          <span id="ib-post-status" style="margin-left:8px;font-size:10px;color:var(--dim)"></span>
        </div>
        <div id="ib-threads"></div>
      </div>
    </div>
)HTML";

    // ─── News Tab ───
    html += R"HTML(
    <div class="tab" id="tab-news">
      <div class="section-title">News</div>
      <div class="section" style="background:var(--bg2);padding:12px;border-radius:3px;margin-bottom:12px">
        <div class="form-row"><input id="news-title" placeholder="Title"><input id="news-url" placeholder="URL (optional)" style="margin-left:8px"><button class="btn btn-sm" onclick="submitNews()" style="margin-left:8px">Submit</button></div>
      </div>
      <div id="news-list"></div>
    </div>
)HTML";

    // ─── Market Tab ───
    html += R"HTML(
    <div class="tab" id="tab-market">
      <div class="section-title">Marketplace</div>
      <div class="product-grid" id="market-grid"></div>
    </div>
)HTML";

    // ─── Peers Tab ───
    html += R"HTML(
    <div class="tab" id="tab-peers">
      <div class="section-title">Connected Peers</div>
      <table><thead><tr><th>Address</th><th>Direction</th><th>Version</th></tr></thead><tbody id="peers-list"></tbody></table>
      <button class="btn btn-sm" style="margin-top:12px" onclick="refreshPeers()">Refresh</button>
    </div>
)HTML";

    // ─── Nodes Tab ───
    html += R"HTML(
    <div class="tab" id="tab-nodes">
      <div class="section-title">Node Control Panel</div>
      <div style="margin-bottom:12px;color:var(--dim);font-size:11px">Manage your bnet cluster. Add remote nodes to monitor and control them from here.</div>
      <div class="form-row" style="margin-bottom:16px">
        <input id="node-name" placeholder="Name (e.g. node-0)" style="width:120px">
        <input id="node-host" placeholder="Host:Port (e.g. node-0.local:9332)" style="width:220px;margin-left:8px">
        <button class="btn btn-sm" onclick="addNode()" style="margin-left:8px">Add Node</button>
      </div>
      <div id="nodes-grid" class="product-grid"></div>
    </div>
)HTML";

    html += "</div></div>"; // close content + main

    // ─── JavaScript ───
    html += "<script>";
    html += R"JS(
// RPC helper
async function rpc(method, params=[]) {
  try {
    const r = await fetch('http://localhost:9332', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({method, params, id: Date.now()})
    });
    const j = await r.json();
    if (j.error) throw new Error(j.error.message);
    return j.result;
  } catch(e) { console.warn('RPC error:', method, e); return null; }
}

// Tab switching
function showTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
  const tab = document.getElementById('tab-'+name);
  if (tab) tab.classList.add('active');
  event.currentTarget.classList.add('active');
  if (name === 'wallet') refreshWallet();
  if (name === 'peers') refreshPeers();
  if (name === 'news') refreshNews();
  if (name === 'market') refreshMarket();
  if (name === 'imageboard') refreshBoard();
  if (name === 'nodes') refreshNodes();
}

// Clock
setInterval(() => {
  document.getElementById('clock').textContent = new Date().toLocaleTimeString();
}, 1000);

// Block-found pop sound via Web Audio API
let lastKnownHeight = 0;
function playBlockPop() {
  try {
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.type = 'sine';
    osc.frequency.setValueAtTime(880, ctx.currentTime);
    osc.frequency.exponentialRampToValueAtTime(1760, ctx.currentTime + 0.05);
    osc.frequency.exponentialRampToValueAtTime(440, ctx.currentTime + 0.15);
    gain.gain.setValueAtTime(0.3, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.01, ctx.currentTime + 0.2);
    osc.start(ctx.currentTime);
    osc.stop(ctx.currentTime + 0.2);
  } catch(e) {}
}

// Dashboard refresh
async function refreshDashboard() {
  const info = await rpc('getmininginfo');
  if (!info) return;
  const newHeight = info.blocks || 0;
  if (lastKnownHeight > 0 && newHeight > lastKnownHeight) playBlockPop();
  lastKnownHeight = newHeight;
  document.getElementById('dash-balance').textContent = info.balance || '0.00';
  document.getElementById('dash-height').textContent = info.blocks || 0;
  document.getElementById('dash-connections').textContent = info.connections || 0;
  document.getElementById('dash-difficulty').textContent = (info.difficulty || 0).toFixed(4);
  document.getElementById('dash-mining').textContent = info.generate ? 'ON' : 'OFF';
  document.getElementById('dash-mining').style.color = info.generate ? 'var(--green)' : 'var(--dim)';
  document.getElementById('dash-mining-sub').textContent = info.generate ? 'generating' : 'idle';

  let hr = info.hashrate || 0;
  let unit = 'H/s';
  if (hr > 1e9) { hr /= 1e9; unit = 'GH/s'; }
  else if (hr > 1e6) { hr /= 1e6; unit = 'MH/s'; }
  else if (hr > 1e3) { hr /= 1e3; unit = 'KH/s'; }
  document.getElementById('dash-hashrate').textContent = hr.toFixed(1);
  document.getElementById('dash-hashrate').nextElementSibling.textContent = unit;

  // Topbar
  document.getElementById('tb-connections').textContent = info.connections || 0;
  document.getElementById('tb-height').textContent = info.blocks || 0;
  document.getElementById('tb-balance').textContent = (info.balance || '0.00') + ' BC';

  // Recent transactions
  const txs = await rpc('listtransactions');
  if (txs) {
    const tbody = document.getElementById('dash-txlist');
    tbody.innerHTML = txs.slice(0, 10).map(tx => {
      const cls = tx.amount.startsWith('-') ? 'amount-neg' : 'amount-pos';
      const d = new Date(tx.time * 1000);
      const time = d.toLocaleDateString() + ' ' + d.toLocaleTimeString();
      return '<tr><td>'+time+'</td><td>'+tx.type+'</td><td style="font-size:10px">'+(tx.address||'').substring(0,20)+'...</td><td class="'+cls+'">'+tx.amount+'</td><td>'+tx.confirmations+'</td></tr>';
    }).join('');
  }

  // Recent blocks table
  const blocks = await rpc('getrecentblocks');
  if (blocks) {
    const tbody = document.getElementById('dash-blocks');
    tbody.innerHTML = blocks.map(b => {
      const d = new Date(b.time * 1000);
      const time = d.toLocaleTimeString();
      const delta = b.timedelta > 0 ? b.timedelta + 's' : '-';
      const shortHash = b.hash.substring(0, 16) + '...';
      return '<tr><td style="color:var(--green);font-weight:700">'+b.height+'</td><td style="font-size:10px;font-family:monospace;cursor:pointer;color:var(--dim)" onclick="showTab(\'explorer\');document.getElementById(\'explorer-search\').value=\''+b.hash+'\';doExplore()">'+shortHash+'</td><td>'+time+'</td><td>'+delta+'</td><td>'+b.txcount+'</td><td style="font-size:10px;color:var(--dim)">'+b.nonce+'</td></tr>';
    }).join('');
  }
}

// Wallet
async function refreshWallet() {
  const txs = await rpc('listtransactions');
  if (!txs) return;
  const tbody = document.getElementById('wallet-txlist');
  tbody.innerHTML = txs.map(tx => {
    const cls = tx.amount.startsWith('-') ? 'amount-neg' : 'amount-pos';
    const d = new Date(tx.time * 1000);
    return '<tr><td>'+d.toLocaleDateString()+' '+d.toLocaleTimeString()+'</td><td>'+tx.type+'</td><td style="font-size:10px">'+(tx.address||'')+'</td><td class="'+cls+'">'+tx.amount+'</td><td>'+tx.confirmations+'</td></tr>';
  }).join('');
}

async function doSend() {
  const addr = document.getElementById('send-addr').value;
  const amt = document.getElementById('send-amount').value;
  if (!addr || !amt) return;
  const r = await rpc('sendtoaddress', [addr, amt]);
  document.getElementById('send-result').innerHTML = r ? '<span style="color:var(--green)">Sent! TX: '+r.substring(0,16)+'...</span>' : '<span style="color:#f44">Failed</span>';
}

async function newAddr() {
  const addr = await rpc('getnewaddress');
  if (addr) document.getElementById('recv-addr').value = addr;
}

function copyAddr() {
  const addr = document.getElementById('recv-addr').value;
  if (addr) navigator.clipboard.writeText(addr);
}

// Explorer
async function doExplore() {
  const q = document.getElementById('explorer-search').value.trim();
  const out = document.getElementById('explorer-result');
  if (!q) return;

  // Try as block height
  if (/^\d+$/.test(q)) {
    const hash = await rpc('getblockhash', [q]);
    if (hash) { showBlock(hash); return; }
  }

  // Try as block hash
  let block = await rpc('getblock', [q]);
  if (block) { renderBlock(block); return; }

  // Try as tx
  let tx = await rpc('getrawtransaction', [q]);
  if (tx) { renderTx(tx); return; }

  out.innerHTML = '<span style="color:#f44">Not found</span>';
}

function renderBlock(b) {
  const out = document.getElementById('explorer-result');
  out.innerHTML = '<div class="card" style="margin-bottom:8px"><div class="card-label">Block '+b.height+'</div><div style="font-size:10px;word-break:break-all;color:var(--green)">'+b.hash+'</div></div>' +
    '<table><tr><td style="color:var(--dim)">Time</td><td>'+new Date(b.time*1000).toLocaleString()+'</td></tr>' +
    '<tr><td style="color:var(--dim)">Txns</td><td>'+b.txcount+'</td></tr>' +
    '<tr><td style="color:var(--dim)">Nonce</td><td>'+b.nonce+'</td></tr>' +
    '<tr><td style="color:var(--dim)">Prev</td><td style="font-size:10px;cursor:pointer;color:var(--green)" onclick="showBlock(\''+b.previousblockhash+'\')">'+b.previousblockhash+'</td></tr>' +
    '</table>' +
    '<div class="section-title" style="margin-top:12px">Transactions</div>' +
    b.tx.map(t => '<div style="padding:4px 0;font-size:10px;cursor:pointer;color:var(--text)" onclick="showTx(\''+t.txid+'\')">'+t.txid+(t.coinbase?' <span style="color:var(--green)">[coinbase '+t.value+']</span>':'')+'</div>').join('');
}

async function showBlock(hash) {
  const b = await rpc('getblock', [hash]);
  if (b) renderBlock(b);
}

function renderTx(tx) {
  const out = document.getElementById('explorer-result');
  out.innerHTML = '<div class="card"><div class="card-label">Transaction</div><div style="font-size:10px;word-break:break-all;color:var(--green)">'+tx.txid+'</div></div>' +
    '<div class="section-title">Outputs</div>' +
    tx.vout.map(v => '<div style="padding:4px 0"><span class="amount-pos">'+v.value+'</span> -> <span style="font-size:10px">'+(v.address||'script')+'</span></div>').join('');
}

async function showTx(txid) {
  const tx = await rpc('getrawtransaction', [txid]);
  if (tx) renderTx(tx);
}

// Chess board
const PIECES = {P:'\u2659',N:'\u2658',B:'\u2657',R:'\u2656',Q:'\u2655',K:'\u2654',
                p:'\u265F',n:'\u265E',b:'\u265D',r:'\u265C',q:'\u265B',k:'\u265A'};
const START_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR';

function renderChessBoard(fen) {
  const board = document.getElementById('chess-board');
  const rows = (fen || START_FEN).split('/');
  let html = '';
  for (let r = 0; r < 8; r++) {
    let col = 0;
    for (const c of rows[r]) {
      if (c >= '1' && c <= '8') {
        for (let i = 0; i < parseInt(c); i++) {
          const light = (r + col) % 2 === 0;
          html += '<div class="chess-sq '+(light?'light':'dark')+'" data-sq="'+'abcdefgh'[col]+(8-r)+'"></div>';
          col++;
        }
      } else {
        const light = (r + col) % 2 === 0;
        html += '<div class="chess-sq '+(light?'light':'dark')+'" data-sq="'+'abcdefgh'[col]+(8-r)+'">'+(PIECES[c]||'')+'</div>';
        col++;
      }
    }
  }
  board.innerHTML = html;
}

function makeChessMove() {
  const move = document.getElementById('chess-move').value;
  if (move) {
    const hist = document.getElementById('chess-history');
    hist.textContent += move + ' ';
    document.getElementById('chess-move').value = '';
  }
}
function challengeChess() { alert('Chess challenges require peer connection'); }

// Poker
function pokerAction(action) { console.log('Poker:', action); }

// Bitboard - CGA 16-color palette
const CGA=[[0,0,0],[0,0,170],[0,170,0],[0,170,170],[170,0,0],[170,0,170],[170,170,0],[170,170,170],
  [85,85,85],[85,85,255],[85,255,85],[85,255,255],[255,85,85],[255,85,255],[255,255,85],[255,255,255]];

function nearCGA(r,g,b) {
  let best=0, bd=1e9;
  for (let i=0; i<16; i++) {
    const d=(r-CGA[i][0])**2 + (g-CGA[i][1])**2 + (b-CGA[i][2])**2;
    if (d<bd) { bd=d; best=i; }
  }
  return best;
}

// Floyd-Steinberg error diffusion
function ditherFS(rgb, w, h) {
  const img = new Int16Array(w*h*3);
  for (let i=0; i<w*h*3; i++) img[i] = rgb[i];
  const out = new Uint8Array(w*h);
  for (let y=0; y<h; y++) for (let x=0; x<w; x++) {
    const i = (y*w+x)*3;
    const r = Math.max(0, Math.min(255, img[i]));
    const g = Math.max(0, Math.min(255, img[i+1]));
    const b = Math.max(0, Math.min(255, img[i+2]));
    const c = nearCGA(r, g, b);
    out[y*w+x] = c;
    const er = r-CGA[c][0], eg = g-CGA[c][1], eb = b-CGA[c][2];
    if (x+1<w) { img[i+3]+=er*7/16; img[i+4]+=eg*7/16; img[i+5]+=eb*7/16; }
    if (y+1<h) {
      if (x>0) { const j=((y+1)*w+(x-1))*3; img[j]+=er*3/16; img[j+1]+=eg*3/16; img[j+2]+=eb*3/16; }
      { const j=((y+1)*w+x)*3; img[j]+=er*5/16; img[j+1]+=eg*5/16; img[j+2]+=eb*5/16; }
      if (x+1<w) { const j=((y+1)*w+(x+1))*3; img[j]+=er/16; img[j+1]+=eg/16; img[j+2]+=eb/16; }
    }
  }
  return out;
}

// Bayer 4x4 ordered dithering
function ditherOrd(rgb, w, h) {
  const B = [[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]];
  const out = new Uint8Array(w*h);
  for (let y=0; y<h; y++) for (let x=0; x<w; x++) {
    const i = (y*w+x)*3;
    const t = (B[y%4][x%4]/16 - 0.5) * 64;
    out[y*w+x] = nearCGA(
      Math.max(0, Math.min(255, rgb[i]+t)),
      Math.max(0, Math.min(255, rgb[i+1]+t)),
      Math.max(0, Math.min(255, rgb[i+2]+t)));
  }
  return out;
}

// Atkinson dithering (classic Mac style - diffuses 3/4 of error)
function ditherAtk(rgb, w, h) {
  const img = new Int16Array(w*h*3);
  for (let i=0; i<w*h*3; i++) img[i] = rgb[i];
  const out = new Uint8Array(w*h);
  for (let y=0; y<h; y++) for (let x=0; x<w; x++) {
    const i = (y*w+x)*3;
    const r = Math.max(0, Math.min(255, img[i]));
    const g = Math.max(0, Math.min(255, img[i+1]));
    const b = Math.max(0, Math.min(255, img[i+2]));
    const c = nearCGA(r, g, b);
    out[y*w+x] = c;
    const er = (r-CGA[c][0])/8, eg = (g-CGA[c][1])/8, eb = (b-CGA[c][2])/8;
    for (const [dx,dy] of [[1,0],[2,0],[-1,1],[0,1],[1,1],[0,2]]) {
      const nx=x+dx, ny=y+dy;
      if (nx>=0 && nx<w && ny<h) { const j=(ny*w+nx)*3; img[j]+=er; img[j+1]+=eg; img[j+2]+=eb; }
    }
  }
  return out;
}

// Nearest color only (no dithering)
function ditherNone(rgb, w, h) {
  const out = new Uint8Array(w*h);
  for (let y=0; y<h; y++) for (let x=0; x<w; x++) {
    const i = (y*w+x)*3;
    out[y*w+x] = nearCGA(rgb[i], rgb[i+1], rgb[i+2]);
  }
  return out;
}

function doDither(rgb, w, h, algo) {
  switch(algo) {
    case 'ordered': return ditherOrd(rgb, w, h);
    case 'atkinson': return ditherAtk(rgb, w, h);
    case 'none': return ditherNone(rgb, w, h);
    default: return ditherFS(rgb, w, h);
  }
}

// Render indexed CGA pixels to a canvas
function renderIdx(canvas, idx, w, h, scale) {
  const s = scale || 1;
  canvas.width = w * s; canvas.height = h * s;
  const ctx = canvas.getContext('2d');
  const id = ctx.createImageData(w, h);
  for (let i=0; i<w*h; i++) {
    const c = CGA[idx[i] || 0];
    id.data[i*4]=c[0]; id.data[i*4+1]=c[1]; id.data[i*4+2]=c[2]; id.data[i*4+3]=255;
  }
  if (s > 1) {
    const tc = document.createElement('canvas'); tc.width=w; tc.height=h;
    tc.getContext('2d').putImageData(id, 0, 0);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(tc, 0, 0, w*s, h*s);
  } else { ctx.putImageData(id, 0, 0); }
}

let ibImgData = { main: null, reply: null };
let currentBoard = '/b/';
let currentThread = null;

// Handle image file selection - auto-generate dithered preview
function onImageSelect(input, which) {
  if (!input.files[0]) {
    ibImgData[which] = null;
    document.getElementById(which==='main' ? 'ib-preview' : 'ib-reply-preview').style.display = 'none';
    return;
  }
  const reader = new FileReader();
  reader.onload = function(e) {
    const img = new Image();
    img.onload = function() {
      const MAX = 128;
      let w = img.width, h = img.height;
      if (w > MAX) { h = Math.round(h*MAX/w); w = MAX; }
      if (h > MAX) { w = Math.round(w*MAX/h); h = MAX; }
      const cv = document.createElement('canvas'); cv.width=w; cv.height=h;
      const cx = cv.getContext('2d'); cx.drawImage(img, 0, 0, w, h);
      const d = cx.getImageData(0, 0, w, h).data;
      const rgb = new Uint8Array(w*h*3);
      for (let i=0; i<w*h; i++) { rgb[i*3]=d[i*4]; rgb[i*3+1]=d[i*4+1]; rgb[i*3+2]=d[i*4+2]; }
      ibImgData[which] = { rgb, w, h };
      // Render original thumbnail
      const origCv = document.getElementById(which==='main' ? 'ib-prev-orig' : 'ib-reply-prev-orig');
      origCv.width=w; origCv.height=h; origCv.getContext('2d').drawImage(img, 0, 0, w, h);
      origCv.style.width = Math.max(w*2,64)+'px'; origCv.style.height = Math.max(h*2,64)+'px';
      updatePreview(which);
    };
    img.src = e.target.result;
  };
  reader.readAsDataURL(input.files[0]);
}

// Apply selected dithering algorithm and show preview
function updatePreview(which) {
  const d = ibImgData[which]; if (!d) return;
  const algo = document.getElementById(which==='main' ? 'ib-dither-algo' : 'ib-reply-dither').value;
  const idx = doDither(d.rgb, d.w, d.h, algo);
  ibImgData[which].indexed = idx;
  const cv = document.getElementById(which==='main' ? 'ib-prev-dith' : 'ib-reply-prev-dith');
  renderIdx(cv, idx, d.w, d.h);
  cv.style.width = Math.max(d.w*2,64)+'px'; cv.style.height = Math.max(d.h*2,64)+'px';
  const info = document.getElementById(which==='main' ? 'ib-prev-info' : 'ib-reply-prev-info');
  info.textContent = d.w+'x'+d.h+' | '+algo+' | 16 colors CGA';
  document.getElementById(which==='main' ? 'ib-preview' : 'ib-reply-preview').style.display = 'block';
}

// Generate dithered BNET logo for Bitboard tab header
function renderBnetLogo() {
  const cv = document.createElement('canvas'); cv.width=140; cv.height=32;
  const cx = cv.getContext('2d');
  cx.fillStyle='#000'; cx.fillRect(0,0,140,32);
  cx.font='bold 24px monospace'; cx.fillStyle='#FFD700'; cx.textBaseline='middle'; cx.fillText('BNET',6,14);
  cx.font='10px monospace'; cx.fillStyle='#00aa00'; cx.fillText('bitboard',72,26);
  const d = cx.getImageData(0,0,140,32).data;
  const rgb = new Uint8Array(140*32*3);
  for (let i=0; i<140*32; i++) { rgb[i*3]=d[i*4]; rgb[i*3+1]=d[i*4+1]; rgb[i*3+2]=d[i*4+2]; }
  const idx = ditherFS(rgb, 140, 32);
  renderIdx(document.getElementById('ib-logo'), idx, 140, 32, 3);
}

function switchBoard(board) {
  currentBoard = board;
  document.querySelectorAll('.board-tab').forEach(t => t.classList.toggle('active', t.textContent === board));
  closeThread(); refreshBoard();
}

async function refreshBoard() {
  const threads = await rpc('listthreads', [currentBoard]);
  const el = document.getElementById('ib-threads');
  if (!threads || !threads.length) {
    el.innerHTML = '<div style="color:var(--dim);padding:20px;text-align:center">No threads on '+currentBoard+' yet. Be the first to post.</div>';
    return;
  }
  el.innerHTML = threads.map(t => {
    const d = new Date(t.time * 1000);
    const time = d.toLocaleDateString() + ' ' + d.toLocaleTimeString();
    return '<div style="background:var(--bg2);border:1px solid var(--border);border-radius:3px;padding:12px;margin-bottom:8px;cursor:pointer" onclick="openThread(\''+t.hash+'\')">' +
      '<div style="display:flex;justify-content:space-between;align-items:center">' +
      '<div style="font-weight:700;color:var(--white)">'+(t.subject||'(no subject)')+'</div>' +
      '<div style="font-size:10px;color:var(--dim)">'+(t.hasimage?'[img] ':'')+t.replies+' replies</div></div>' +
      '<div style="margin-top:4px;font-size:12px;color:var(--text)">'+t.comment.substring(0,200)+(t.comment.length>200?'...':'')+'</div>' +
      '<div style="margin-top:4px;font-size:10px;color:var(--dim)"><span style="color:var(--green)">'+t.tripcode+'</span> &middot; '+time+'</div></div>';
  }).join('');
}

// Load and render an on-chain dithered image
async function loadPostImage(hash, container) {
  const r = await rpc('getpostimage', [hash]);
  if (!r || !r.width) return;
  const b = atob(r.pixels);
  const idx = new Uint8Array(b.length);
  for (let i=0; i<b.length; i++) idx[i] = b.charCodeAt(i);
  const cv = document.createElement('canvas');
  cv.style.imageRendering = 'pixelated';
  cv.style.border = '1px solid var(--border)';
  cv.style.marginTop = '8px'; cv.style.display = 'block';
  renderIdx(cv, idx, r.width, r.height);
  cv.style.width = Math.max(r.width*2, 64)+'px'; cv.style.height = Math.max(r.height*2, 64)+'px';
  container.appendChild(cv);
}

async function openThread(hash) {
  currentThread = hash;
  document.getElementById('ib-board-view').style.display = 'none';
  document.getElementById('ib-thread-view').style.display = 'block';
  const thread = await rpc('getthread', [hash]);
  if (!thread) return;
  const op = thread.op;
  const opTime = new Date(op.time * 1000);
  let html = '<div style="background:var(--bg2);border:1px solid var(--border);border-radius:3px;padding:12px;margin-bottom:8px">';
  html += '<div style="font-weight:700;font-size:15px;color:var(--white)">'+(op.subject||'(no subject)')+'</div>';
  html += '<div style="font-size:10px;color:var(--dim);margin:4px 0"><span style="color:var(--green)">'+op.tripcode+'</span> &middot; '+opTime.toLocaleString()+'</div>';
  if (op.hasimage) html += '<div class="post-img" data-hash="'+op.hash+'"></div>';
  html += '<div style="margin-top:8px;white-space:pre-wrap">'+op.comment+'</div></div>';
  if (thread.replies && thread.replies.length) {
    thread.replies.forEach((r, i) => {
      const rt = new Date(r.time * 1000);
      html += '<div style="background:var(--bg3);border-left:2px solid var(--border);padding:10px;margin:4px 0 4px 16px;border-radius:2px">';
      html += '<div style="font-size:10px;color:var(--dim)"><span style="color:var(--green)">'+r.tripcode+'</span> &middot; #'+(i+1)+' &middot; '+rt.toLocaleString()+'</div>';
      if (r.hasimage) html += '<div class="post-img" data-hash="'+r.hash+'"></div>';
      html += '<div style="margin-top:4px;white-space:pre-wrap">'+r.comment+'</div></div>';
    });
  }
  document.getElementById('ib-thread-content').innerHTML = html;
  // Load all on-chain images
  document.querySelectorAll('.post-img').forEach(el => { loadPostImage(el.dataset.hash, el); });
}

function closeThread() {
  currentThread = null;
  document.getElementById('ib-board-view').style.display = 'block';
  document.getElementById('ib-thread-view').style.display = 'none';
}

async function postThread() {
  const subj = document.getElementById('ib-subject').value;
  const comm = document.getElementById('ib-comment').value;
  const fileInput = document.getElementById('ib-image');
  if (!comm && !fileInput.files[0]) return;
  const status = document.getElementById('ib-post-status');
  status.textContent = 'Processing...'; status.style.color = 'var(--dim)';
  const d = ibImgData.main;
  let params = [currentBoard, subj, comm || ' ', ''];
  if (d && d.indexed) {
    let b64 = ''; for (let i=0; i<d.indexed.length; i++) b64 += String.fromCharCode(d.indexed[i]);
    params.push(btoa(b64), String(d.w), String(d.h), 'indexed');
    status.textContent = 'Posting with dithered image ('+d.w+'x'+d.h+')...';
  } else { status.textContent = 'Posting...'; }
  const r = await rpc('createpost', params);
  if (r) {
    status.textContent = 'Posted! (on-chain tx broadcast)'; status.style.color = 'var(--green)';
    document.getElementById('ib-subject').value = ''; document.getElementById('ib-comment').value = '';
    fileInput.value = ''; ibImgData.main = null;
    document.getElementById('ib-preview').style.display = 'none';
    setTimeout(refreshBoard, 500);
  } else { status.textContent = 'Failed to post'; status.style.color = 'red'; }
}

async function postReply() {
  if (!currentThread) return;
  const comm = document.getElementById('ib-reply').value;
  const fileInput = document.getElementById('ib-reply-image');
  if (!comm && !fileInput.files[0]) return;
  const status = document.getElementById('ib-reply-status');
  status.textContent = 'Posting reply...';
  const d = ibImgData.reply;
  let params = [currentBoard, '', comm || ' ', currentThread];
  if (d && d.indexed) {
    let b64 = ''; for (let i=0; i<d.indexed.length; i++) b64 += String.fromCharCode(d.indexed[i]);
    params.push(btoa(b64), String(d.w), String(d.h), 'indexed');
  }
  const r = await rpc('createpost', params);
  if (r) {
    status.textContent = 'Reply posted!'; status.style.color = 'var(--green)';
    document.getElementById('ib-reply').value = ''; fileInput.value = '';
    ibImgData.reply = null; document.getElementById('ib-reply-preview').style.display = 'none';
    setTimeout(() => openThread(currentThread), 500);
  } else { status.textContent = 'Failed'; status.style.color = 'red'; }
}

// News
async function refreshNews() {
  const news = await rpc('listnews');
  const list = document.getElementById('news-list');
  if (!news || !news.length) { list.innerHTML = '<div style="color:var(--dim)">No news yet. Submit something!</div>'; return; }
  list.innerHTML = news.map((n,i) => {
    const age = Math.floor((Date.now()/1000 - n.time) / 3600);
    return '<div class="news-item">' +
      '<div class="news-vote"><span class="arrow" onclick="voteNews(\''+n.hash+'\')">&#9650;</span><span>'+n.votes+'</span></div>' +
      '<div class="news-content"><div class="news-title">'+(i+1)+'. '+(n.url ? '<a href="'+n.url+'" target="_blank">'+n.title+'</a>' : n.title)+'</div>' +
      '<div class="news-meta">'+n.score.toFixed(1)+' points | by '+n.author.substring(0,12)+'... | '+age+'h ago</div></div></div>';
  }).join('');
}

async function voteNews(hash) { await rpc('votenews', [hash, 'true']); refreshNews(); }

async function submitNews() {
  const title = document.getElementById('news-title').value;
  const url = document.getElementById('news-url').value;
  if (!title) return;
  await rpc('submitnews', [title, url, '']);
  document.getElementById('news-title').value = '';
  document.getElementById('news-url').value = '';
  refreshNews();
}

// Market
async function refreshMarket() {
  const products = await rpc('listproducts');
  const grid = document.getElementById('market-grid');
  if (!products || !products.length) { grid.innerHTML = '<div style="color:var(--dim)">No products listed yet.</div>'; return; }
  grid.innerHTML = products.map(p =>
    '<div class="product-card"><div class="product-title">'+p.title+'</div>' +
    '<div class="product-price">'+p.price+' BC</div>' +
    '<div class="product-seller">by '+p.seller.substring(0,16)+'...</div></div>'
  ).join('');
}

// Peers
async function refreshPeers() {
  const peers = await rpc('listpeers');
  const tbody = document.getElementById('peers-list');
  if (!peers || !peers.length) { tbody.innerHTML = '<tr><td colspan="3" style="color:var(--dim)">No peers connected</td></tr>'; return; }
  tbody.innerHTML = peers.map(p =>
    '<tr><td>'+p.addr+'</td><td>'+(p.inbound?'inbound':'outbound')+'</td><td>'+p.version+'</td></tr>'
  ).join('');
}

// Node management
let nodes = JSON.parse(localStorage.getItem('bnet_nodes') || '[]');

function addNode() {
  const name = document.getElementById('node-name').value.trim();
  const host = document.getElementById('node-host').value.trim();
  if (!name || !host) return;
  const url = host.includes('://') ? host : 'http://' + host;
  nodes.push({name, url});
  localStorage.setItem('bnet_nodes', JSON.stringify(nodes));
  document.getElementById('node-name').value = '';
  document.getElementById('node-host').value = '';
  refreshNodes();
}

function removeNode(i) {
  nodes.splice(i, 1);
  localStorage.setItem('bnet_nodes', JSON.stringify(nodes));
  refreshNodes();
}

async function nodeRpc(url, method, params=[]) {
  try {
    const r = await fetch(url, {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({method, params, id: Date.now()})
    });
    const j = await r.json();
    return j.result;
  } catch(e) { return null; }
}

async function nodeAction(i, action) {
  const node = nodes[i];
  if (action === 'stop') {
    if (!confirm('Stop mining on ' + node.name + '?')) return;
    await nodeRpc(node.url, 'setgenerate', ['false']);
  } else if (action === 'start') {
    await nodeRpc(node.url, 'setgenerate', ['true']);
  }
  setTimeout(() => refreshNodes(), 500);
}

async function refreshNodes() {
  const grid = document.getElementById('nodes-grid');
  if (!nodes.length) {
    grid.innerHTML = '<div style="color:var(--dim)">No remote nodes configured. Add nodes above.</div>';
    return;
  }
  // Add localhost as "this node" first
  let cards = '';
  // Local node
  const localInfo = await rpc('getinfo');
  const localMining = await rpc('getmininginfo');
  cards += '<div class="product-card" style="border-left:3px solid var(--green)">';
  cards += '<div class="product-title" style="color:var(--green)">localhost (this node)</div>';
  if (localInfo) {
    cards += '<div style="font-size:11px;margin:8px 0;line-height:1.8">';
    cards += 'Height: <span style="color:var(--white)">'+localInfo.blocks+'</span><br>';
    cards += 'Peers: <span style="color:var(--white)">'+localInfo.connections+'</span><br>';
    cards += 'Balance: <span style="color:var(--green)">'+localInfo.balance+' BC</span><br>';
    if (localMining) {
      cards += 'Mining: <span style="color:'+(localMining.generate?'var(--green)':'var(--dim)') +'">'+(localMining.generate?'ON':'OFF')+'</span><br>';
      cards += 'Hashrate: <span style="color:var(--white)">'+formatHashrate(localMining.hashrate)+'</span><br>';
      cards += 'Threads: <span style="color:var(--white)">'+(localMining.threads||'?')+'</span>';
    }
    cards += '</div>';
  } else {
    cards += '<div style="color:red;font-size:11px;margin-top:8px">offline</div>';
  }
  cards += '</div>';

  // Remote nodes
  for (let i = 0; i < nodes.length; i++) {
    const n = nodes[i];
    const info = await nodeRpc(n.url, 'getinfo');
    const mining = await nodeRpc(n.url, 'getmininginfo');
    cards += '<div class="product-card">';
    cards += '<div style="display:flex;justify-content:space-between;align-items:center">';
    cards += '<div class="product-title">'+n.name+'</div>';
    cards += '<span style="cursor:pointer;color:var(--dim);font-size:16px" onclick="removeNode('+i+')" title="Remove">&times;</span>';
    cards += '</div>';
    cards += '<div style="font-size:10px;color:var(--dim);margin-bottom:4px">'+n.url+'</div>';
    if (info) {
      cards += '<div style="font-size:11px;margin:8px 0;line-height:1.8">';
      cards += 'Height: <span style="color:var(--white)">'+info.blocks+'</span><br>';
      cards += 'Peers: <span style="color:var(--white)">'+info.connections+'</span><br>';
      cards += 'Balance: <span style="color:var(--green)">'+info.balance+' BC</span><br>';
      if (mining) {
        cards += 'Mining: <span style="color:'+(mining.generate?'var(--green)':'var(--dim)')+'">'+(mining.generate?'ON':'OFF')+'</span><br>';
        cards += 'Hashrate: <span style="color:var(--white)">'+formatHashrate(mining.hashrate)+'</span><br>';
        cards += 'Threads: <span style="color:var(--white)">'+(mining.threads||'?')+'</span>';
      }
      cards += '</div>';
      cards += '<div style="display:flex;gap:4px;margin-top:8px">';
      if (mining && mining.generate)
        cards += '<button class="btn btn-sm" onclick="nodeAction('+i+',\'stop\')">Stop Mining</button>';
      else
        cards += '<button class="btn btn-sm" onclick="nodeAction('+i+',\'start\')">Start Mining</button>';
      cards += '</div>';
    } else {
      cards += '<div style="color:red;font-size:11px;margin-top:8px">offline / unreachable</div>';
    }
    cards += '</div>';
  }
  grid.innerHTML = cards;
}

function formatHashrate(h) {
  if (!h || h === 0) return '0 H/s';
  if (h >= 1e9) return (h/1e9).toFixed(2)+' GH/s';
  if (h >= 1e6) return (h/1e6).toFixed(2)+' MH/s';
  if (h >= 1e3) return (h/1e3).toFixed(2)+' KH/s';
  return h+' H/s';
}

// Init
renderChessBoard();
refreshDashboard();
newAddr();
renderBnetLogo();
setInterval(refreshDashboard, 5000);
)JS";

    html += "</script></body></html>";
    return html;
}


///
/// Read Content-Length body from HTTP request
///
string ReadHTTPBody(SOCKET hSocket, const string& strHeaders)
{
    string strBody;

    // Find Content-Length
    size_t pos = strHeaders.find("Content-Length:");
    if (pos == string::npos)
        pos = strHeaders.find("content-length:");
    if (pos != string::npos)
    {
        pos = strHeaders.find(':', pos);
        if (pos != string::npos)
        {
            pos++;
            while (pos < strHeaders.size() && strHeaders[pos] == ' ')
                pos++;
            int nContentLength = atoi(strHeaders.c_str() + pos);

            // Find existing body content after headers
            size_t bodyStart = strHeaders.find("\r\n\r\n");
            string strExisting;
            if (bodyStart != string::npos)
                strExisting = strHeaders.substr(bodyStart + 4);

            strBody = strExisting;

            // Read remaining body bytes
            while ((int)strBody.size() < nContentLength)
            {
                char buf[4096];
                int nToRead = min((int)sizeof(buf) - 1, nContentLength - (int)strBody.size());
                int n = recv(hSocket, buf, nToRead, 0);
                if (n <= 0)
                    break;
                buf[n] = '\0';
                strBody += string(buf, n);
            }
        }
    }

    return strBody;
}


///
/// RPC server thread - listens on localhost:9332
///
void ThreadRPCServer(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadRPCServer(parg));

    printf("RPC: starting server thread\n");

    SOCKET hListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        printf("RPC: socket() failed\n");
        return;
    }

    int nOne = 1;
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&nOne, sizeof(nOne));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(9332);

    if (::bind(hListenSocket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("RPC: bind() failed on port 9332\n");
        closesocket(hListenSocket);
        return;
    }
    if (listen(hListenSocket, 5) == SOCKET_ERROR)
    {
        printf("RPC: listen() failed\n");
        closesocket(hListenSocket);
        return;
    }

    printf("RPC server listening on 127.0.0.1:9332\n");

    loop
    {
        if (fShutdown)
            break;

        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        SOCKET hSocket = accept(hListenSocket, (struct sockaddr*)&cliaddr, &clilen);
        if (hSocket == INVALID_SOCKET)
        {
            Sleep(100);
            continue;
        }

        // Only accept connections from localhost
        if (cliaddr.sin_addr.s_addr != htonl(INADDR_LOOPBACK))
        {
            printf("RPC: rejected non-localhost connection\n");
            closesocket(hSocket);
            continue;
        }

        try
        {
            // Read HTTP request headers (read until double newline)
            string strRequest;
            char buf[4096];
            int n;
            while ((n = recv(hSocket, buf, sizeof(buf) - 1, 0)) > 0)
            {
                buf[n] = '\0';
                strRequest += buf;
                if (strRequest.find("\r\n\r\n") != string::npos ||
                    strRequest.find("\n\n") != string::npos)
                    break;
            }

            // Check HTTP method (GET for web UI, OPTIONS for CORS preflight, POST for RPC)
            bool fIsGet = (strRequest.substr(0, 4) == "GET ");
            bool fIsOptions = (strRequest.substr(0, 8) == "OPTIONS ");

            // Handle CORS preflight
            if (fIsOptions)
            {
                string strHTTP =
                    "HTTP/1.1 204 No Content\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type\r\n"
                    "Access-Control-Max-Age: 86400\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                send(hSocket, strHTTP.c_str(), strHTTP.size(), 0);
                closesocket(hSocket);
                continue;
            }

            // Handle GET requests - serve the web UI
            if (fIsGet)
            {
                string strHTML = GetWebUIHTML();
                string strHTTP = strprintf(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    strHTML.size());
                send(hSocket, strHTTP.c_str(), strHTTP.size(), 0);
                send(hSocket, strHTML.c_str(), strHTML.size(), 0);
                closesocket(hSocket);
                continue;
            }

            // Read body based on Content-Length
            string strBody = ReadHTTPBody(hSocket, strRequest);

            // If ReadHTTPBody returned empty but we have data after the header separator,
            // use that as the body (fallback for requests without Content-Length)
            if (strBody.empty())
            {
                size_t bodyStart = strRequest.find("\r\n\r\n");
                if (bodyStart != string::npos)
                    strBody = strRequest.substr(bodyStart + 4);
                else
                {
                    bodyStart = strRequest.find("\n\n");
                    if (bodyStart != string::npos)
                        strBody = strRequest.substr(bodyStart + 2);
                }
            }

            // Parse and dispatch
            string strMethod, strParams, strId;
            string strResponse;

            if (ParseRPCRequest(strBody, strMethod, strParams, strId))
            {
                printf("RPC: method=%s\n", strMethod.c_str());

                if (strMethod == "getinfo")
                    strResponse = JSONResult(HandleGetInfo(), strId);
                else if (strMethod == "getbalance")
                    strResponse = JSONResult(HandleGetBalance(), strId);
                else if (strMethod == "getblockcount")
                    strResponse = JSONResult(HandleGetBlockCount(), strId);
                else if (strMethod == "getnewaddress")
                    strResponse = JSONResult(HandleGetNewAddress(), strId);
                else if (strMethod == "sendtoaddress")
                {
                    string strResult = HandleSendToAddress(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult; // already an error response
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "listproducts")
                    strResponse = JSONResult(HandleListProducts(), strId);
                else if (strMethod == "listnews")
                    strResponse = JSONResult(HandleListNews(), strId);
                else if (strMethod == "submitnews")
                {
                    string strResult = HandleSubmitNews(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "votenews")
                {
                    string strResult = HandleVoteNews(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "getbgoldbalance")
                    strResponse = JSONResult(HandleGetBgoldBalance(), strId);
                else if (strMethod == "getblockhash")
                {
                    string strResult = HandleGetBlockHash(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "getblock")
                {
                    string strResult = HandleGetBlock(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "getrawtransaction")
                {
                    string strResult = HandleGetRawTransaction(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "getblockchaininfo")
                    strResponse = JSONResult(HandleGetBlockchainInfo(), strId);
                else if (strMethod == "getrecentblocks")
                    strResponse = JSONResult(HandleGetRecentBlocks(), strId);
                else if (strMethod == "getrawmempool")
                    strResponse = JSONResult(HandleGetRawMempool(), strId);
                else if (strMethod == "listtransactions")
                    strResponse = JSONResult(HandleListTransactions(), strId);
                else if (strMethod == "listpeers")
                    strResponse = JSONResult(HandleListPeers(), strId);
                else if (strMethod == "getmininginfo")
                    strResponse = JSONResult(HandleGetMiningInfo(), strId);
                else if (strMethod == "setgenerate")
                    strResponse = JSONResult(HandleSetGenerate(strParams), strId);
                else if (strMethod == "addnode")
                {
                    string strResult = HandleAddNode(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "listthreads")
                    strResponse = JSONResult(HandleListThreads(strParams), strId);
                else if (strMethod == "getthread")
                {
                    string strResult = HandleGetThread(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "createpost")
                {
                    string strResult = HandleCreatePost(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else if (strMethod == "getpostimage")
                {
                    string strResult = HandleGetPostImage(strParams);
                    if (strResult.find("\"error\"") != string::npos)
                        strResponse = strResult;
                    else
                        strResponse = JSONResult(strResult, strId);
                }
                else
                    strResponse = JSONError("Method not found: " + strMethod, strId);
            }
            else
            {
                strResponse = JSONError("Parse error", "null");
            }

            // Send HTTP response with CORS headers
            string strHTTP = strprintf(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n",
                strResponse.size());

            send(hSocket, strHTTP.c_str(), strHTTP.size(), 0);
            send(hSocket, strResponse.c_str(), strResponse.size(), 0);
        }
        catch (std::exception& e)
        {
            string strErr = JSONError(string("Internal error: ") + e.what(), "null");
            string strHTTP = strprintf(
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n",
                strErr.size());
            send(hSocket, strHTTP.c_str(), strHTTP.size(), 0);
            send(hSocket, strErr.c_str(), strErr.size(), 0);
            printf("RPC: exception: %s\n", e.what());
        }
        catch (...)
        {
            printf("RPC: unknown exception\n");
        }

        closesocket(hSocket);
    }

    closesocket(hListenSocket);
    printf("RPC: server thread exiting\n");
}
