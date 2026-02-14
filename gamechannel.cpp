// Copyright (c) 2026 bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "gamechannel.h"

// Global state
map<uint256, CGameChallenge> mapGameChallenges;
map<uint256, CGameSession> mapGameSessions;
CCriticalSection cs_mapGames;


// Create a 2-of-2 multisig script: OP_2 <pubkeyA> <pubkeyB> OP_2 OP_CHECKMULTISIG
CScript CreateMultisigScript(const vector<unsigned char>& pubkeyA, const vector<unsigned char>& pubkeyB)
{
    CScript script;
    script << OP_2;
    script << pubkeyA;
    script << pubkeyB;
    script << OP_2;
    script << OP_CHECKMULTISIG;
    return script;
}


// Accept and store a game challenge from the network
bool AcceptGameChallenge(const CGameChallenge& challenge)
{
    CRITICAL_BLOCK(cs_mapGames)
    {
        uint256 hash = challenge.GetHash();
        if (mapGameChallenges.count(hash))
            return false; // already known

        // Validate
        if (challenge.nGameType != GAME_CHESS && challenge.nGameType != GAME_POKER)
            return error("AcceptGameChallenge() : unknown game type %d", challenge.nGameType);

        if (challenge.nBetAmount < 0)
            return error("AcceptGameChallenge() : negative bet amount");

        if (challenge.vchPubKey.empty())
            return error("AcceptGameChallenge() : empty pubkey");

        if (!challenge.CheckSignature())
            return error("AcceptGameChallenge() : invalid signature");

        mapGameChallenges[hash] = challenge;
        printf("AcceptGameChallenge() : accepted challenge %s type=%d bet=%lld\n",
            hash.ToString().substr(0,14).c_str(), challenge.nGameType, challenge.nBetAmount);
    }
    return true;
}


// Process an acceptance of a game challenge - creates a session
bool AcceptGameAccept(const CGameAccept& accept)
{
    CRITICAL_BLOCK(cs_mapGames)
    {
        // Find the referenced challenge
        if (!mapGameChallenges.count(accept.hashChallenge))
            return error("AcceptGameAccept() : challenge %s not found",
                accept.hashChallenge.ToString().substr(0,14).c_str());

        if (!accept.CheckSignature())
            return error("AcceptGameAccept() : invalid signature");

        const CGameChallenge& challenge = mapGameChallenges[accept.hashChallenge];

        // Don't accept your own challenge
        if (accept.vchPubKey == challenge.vchPubKey)
            return error("AcceptGameAccept() : cannot accept own challenge");

        // Create game session
        CGameSession session;
        session.nGameType = challenge.nGameType;
        session.nState = GSTATE_OPEN;
        session.nBetAmount = challenge.nBetAmount;
        session.vchPubKeyA = challenge.vchPubKey;   // challenger plays first (white in chess)
        session.vchPubKeyB = accept.vchPubKey;
        session.hashChallenge = accept.hashChallenge;
        session.nLockTime = nBestHeight + 144; // ~1 day refund locktime

        uint256 hashSession = session.GetHash();
        mapGameSessions[hashSession] = session;

        // Remove the challenge since it's been accepted
        mapGameChallenges.erase(accept.hashChallenge);

        printf("AcceptGameAccept() : created session %s for challenge %s\n",
            hashSession.ToString().substr(0,14).c_str(),
            accept.hashChallenge.ToString().substr(0,14).c_str());
    }
    return true;
}


// Process a game move from the opponent
bool ProcessGameMove(const CGameMove& move)
{
    CRITICAL_BLOCK(cs_mapGames)
    {
        if (!mapGameSessions.count(move.hashSession))
            return error("ProcessGameMove() : session %s not found",
                move.hashSession.ToString().substr(0,14).c_str());

        if (!move.CheckSignature())
            return error("ProcessGameMove() : invalid signature");

        CGameSession& session = mapGameSessions[move.hashSession];

        // Verify the move is from one of the players
        if (move.vchPubKey != session.vchPubKeyA && move.vchPubKey != session.vchPubKeyB)
            return error("ProcessGameMove() : move from unknown player");

        // Verify move number
        if (move.nMoveNumber != (int)session.vMoves.size())
            return error("ProcessGameMove() : unexpected move number %d (expected %d)",
                move.nMoveNumber, (int)session.vMoves.size());

        // Store the move
        session.vMoves.push_back(move);

        // Update state to playing if it was funded
        if (session.nState == GSTATE_FUNDED || session.nState == GSTATE_OPEN)
            session.nState = GSTATE_PLAYING;

        printf("ProcessGameMove() : session %s move #%d: %s\n",
            move.hashSession.ToString().substr(0,14).c_str(),
            move.nMoveNumber, move.strMove.c_str());
    }
    return true;
}


// Process a game settlement
bool ProcessGameSettle(const CGameSettle& settle)
{
    CRITICAL_BLOCK(cs_mapGames)
    {
        if (!mapGameSessions.count(settle.hashSession))
            return error("ProcessGameSettle() : session %s not found",
                settle.hashSession.ToString().substr(0,14).c_str());

        CGameSession& session = mapGameSessions[settle.hashSession];

        // Verify the settlement transaction is valid
        if (!settle.txSettle.CheckTransaction())
            return error("ProcessGameSettle() : invalid settlement transaction");

        // Accept the settlement transaction into the mempool
        bool fMissingInputs = false;
        CTransaction tx = settle.txSettle;
        if (!tx.AcceptTransaction(true, &fMissingInputs))
        {
            if (fMissingInputs)
                printf("ProcessGameSettle() : missing inputs, queuing\n");
            else
                return error("ProcessGameSettle() : settlement tx rejected");
        }

        session.nState = GSTATE_SETTLED;

        printf("ProcessGameSettle() : session %s settled, winner=%s\n",
            settle.hashSession.ToString().substr(0,14).c_str(),
            settle.hashWinner.ToString().substr(0,14).c_str());
    }
    return true;
}


// Create a funding transaction sending bet amount to 2-of-2 multisig
bool CreateFundingTransaction(CGameSession& session, CWalletTx& wtxFunding)
{
    CScript scriptMultisig = CreateMultisigScript(session.vchPubKeyA, session.vchPubKeyB);

    int64 nFeeRequired = 0;
    if (!CreateTransaction(scriptMultisig, session.nBetAmount, wtxFunding, nFeeRequired))
        return error("CreateFundingTransaction() : CreateTransaction failed");

    session.hashFundingTx = wtxFunding.GetHash();
    return true;
}


// Create a refund transaction that spends funding back to player after locktime
bool CreateRefundTransaction(const CGameSession& session, CTransaction& txRefund, bool fPlayerA)
{
    // Build a transaction spending the funding output back to the player
    txRefund.vin.resize(1);
    txRefund.vin[0].prevout = COutPoint(session.hashFundingTx, 0);
    txRefund.nLockTime = session.nLockTime;

    // Output to the player's address
    const vector<unsigned char>& vchPubKey = fPlayerA ? session.vchPubKeyA : session.vchPubKeyB;
    CScript scriptPubKey;
    scriptPubKey << vchPubKey << OP_CHECKSIG;

    txRefund.vout.resize(1);
    txRefund.vout[0].nValue = session.nBetAmount;
    txRefund.vout[0].scriptPubKey = scriptPubKey;

    return true;
}


// Create a settlement transaction that pays the winner immediately
bool CreateSettlementTransaction(const CGameSession& session, CTransaction& txSettle, const vector<unsigned char>& vchPubKeyWinner)
{
    txSettle.vin.resize(1);
    txSettle.vin[0].prevout = COutPoint(session.hashFundingTx, 0);
    txSettle.nLockTime = 0; // immediate, overrides refund

    CScript scriptPubKey;
    scriptPubKey << vchPubKeyWinner << OP_CHECKSIG;

    txSettle.vout.resize(1);
    txSettle.vout[0].nValue = session.nBetAmount * 2; // winner gets both bets
    txSettle.vout[0].scriptPubKey = scriptPubKey;

    return true;
}


// Create and broadcast a game challenge
bool SendGameChallenge(int nGameType, int64 nBetAmount)
{
    CGameChallenge challenge;
    challenge.nGameType = nGameType;
    challenge.nBetAmount = nBetAmount;
    challenge.nTime = GetAdjustedTime();
    challenge.vchPubKey = keyUser.GetPubKey();

    CKey key;
    key.SetPubKey(keyUser.GetPubKey());
    key.SetPrivKey(mapKeys[keyUser.GetPubKey()]);
    if (!challenge.Sign(key))
        return error("SendGameChallenge() : failed to sign challenge");

    AcceptGameChallenge(challenge);

    // Relay to all peers
    CInv inv(MSG_GAME_CHALLENGE, challenge.GetHash());
    CRITICAL_BLOCK(cs_vNodes)
        foreach(CNode* pnode, vNodes)
            pnode->PushMessage("gamechallenge", challenge);

    return true;
}


// Accept a game challenge and notify the challenger
bool SendGameAccept(const uint256& hashChallenge, CNode* pnodeChallenger)
{
    CGameAccept accept;
    accept.hashChallenge = hashChallenge;
    accept.vchPubKey = keyUser.GetPubKey();

    CKey key;
    key.SetPubKey(keyUser.GetPubKey());
    key.SetPrivKey(mapKeys[keyUser.GetPubKey()]);
    if (!accept.Sign(key))
        return error("SendGameAccept() : failed to sign accept");

    AcceptGameAccept(accept);

    // Send directly to the challenger
    if (pnodeChallenger)
        pnodeChallenger->PushMessage("gameaccept", accept);

    return true;
}


// Send a game move to the opponent
bool SendGameMove(const uint256& hashSession, const string& strMove, const vector<unsigned char>& vchPayload)
{
    CRITICAL_BLOCK(cs_mapGames)
    {
        if (!mapGameSessions.count(hashSession))
            return error("SendGameMove() : session not found");

        CGameSession& session = mapGameSessions[hashSession];

        CGameMove move;
        move.hashSession = hashSession;
        move.nMoveNumber = (int)session.vMoves.size();
        move.strMove = strMove;
        move.vchPayload = vchPayload;
        move.vchPubKey = keyUser.GetPubKey();

        CKey key;
        key.SetPubKey(keyUser.GetPubKey());
        key.SetPrivKey(mapKeys[keyUser.GetPubKey()]);
        if (!move.Sign(key))
            return error("SendGameMove() : failed to sign move");

        ProcessGameMove(move);

        // Send directly to opponent
        if (session.pnodeOpponent)
            session.pnodeOpponent->PushMessage("gamemove", move);
    }
    return true;
}
