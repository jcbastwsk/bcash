// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "sha.h"
#include "bgold.h"
#include "rpc.h"


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
        return JSONError("Invalid bcash address", "null");

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
                else if (strMethod == "getrawmempool")
                    strResponse = JSONResult(HandleGetRawMempool(), strId);
                else
                    strResponse = JSONError("Method not found: " + strMethod, strId);
            }
            else
            {
                strResponse = JSONError("Parse error", "null");
            }

            // Send HTTP response
            string strHTTP = strprintf(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
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
