// Copyright (c) 2026 bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMECHANNEL_H
#define GAMECHANNEL_H

class CGameChallenge;
class CGameAccept;
class CGameMove;
class CGameSettle;
class CGameSession;

// Game types
enum GameType
{
    GAME_CHESS = 1,
    GAME_POKER = 2,
};

// Game states
enum GameState
{
    GSTATE_OPEN = 0,
    GSTATE_FUNDED,
    GSTATE_PLAYING,
    GSTATE_FINISHED,
    GSTATE_SETTLED,
    GSTATE_EXPIRED,
};

// A challenge broadcast to find an opponent
class CGameChallenge
{
public:
    int nVersion;
    int nGameType;          // GAME_CHESS or GAME_POKER
    int64 nBetAmount;       // satoshis
    int64 nTime;
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    CGameChallenge()
    {
        nVersion = 1;
        nGameType = 0;
        nBetAmount = 0;
        nTime = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nGameType);
        READWRITE(nBetAmount);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

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

    string ToString() const
    {
        return strprintf("CGameChallenge(type=%d, bet=%lld, time=%lld)",
            nGameType, nBetAmount, nTime);
    }
};

// Acceptance of a challenge
class CGameAccept
{
public:
    int nVersion;
    uint256 hashChallenge;
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    CGameAccept()
    {
        nVersion = 1;
        hashChallenge = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashChallenge);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

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
};

// A single move in a game (sent direct between players)
class CGameMove
{
public:
    int nVersion;
    uint256 hashSession;
    int nMoveNumber;
    string strMove;                    // e.g. "e2e4" for chess, "fold"/"call"/"raise 50000" for poker
    vector<unsigned char> vchPayload; // extra data (card commitments for poker)
    vector<unsigned char> vchPubKey;
    vector<unsigned char> vchSig;

    CGameMove()
    {
        nVersion = 1;
        hashSession = 0;
        nMoveNumber = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashSession);
        READWRITE(nMoveNumber);
        READWRITE(strMove);
        READWRITE(vchPayload);
        READWRITE(vchPubKey);
        if (!(nType & SER_GETHASH))
            READWRITE(vchSig);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
    uint256 GetSigHash() const { return SerializeHash(*this, SER_GETHASH); }

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
};

// Settlement message with the final transaction
class CGameSettle
{
public:
    int nVersion;
    uint256 hashSession;
    uint160 hashWinner;     // Hash160 of winner's pubkey, 0 = draw
    CTransaction txSettle;  // spends the 2-of-2 multisig funding
    vector<unsigned char> vchSigA;
    vector<unsigned char> vchSigB;

    CGameSettle()
    {
        nVersion = 1;
        hashSession = 0;
        hashWinner = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashSession);
        READWRITE(hashWinner);
        READWRITE(txSettle);
        READWRITE(vchSigA);
        READWRITE(vchSigB);
    )

    uint256 GetHash() const { return SerializeHash(*this); }
};

// In-memory game session state
class CGameSession
{
public:
    int nGameType;
    int nState;             // GameState enum
    int64 nBetAmount;
    vector<unsigned char> vchPubKeyA;  // challenger (white in chess)
    vector<unsigned char> vchPubKeyB;  // acceptor
    uint256 hashChallenge;
    uint256 hashFundingTx;
    int nLockTime;
    vector<unsigned char> vchGameState; // serialized chess/poker state
    vector<CGameMove> vMoves;
    CNode* pnodeOpponent;   // direct connection to opponent

    CGameSession()
    {
        nGameType = 0;
        nState = GSTATE_OPEN;
        nBetAmount = 0;
        hashChallenge = 0;
        hashFundingTx = 0;
        nLockTime = 0;
        pnodeOpponent = NULL;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nGameType);
        READWRITE(nState);
        READWRITE(nBetAmount);
        READWRITE(vchPubKeyA);
        READWRITE(vchPubKeyB);
        READWRITE(hashChallenge);
        READWRITE(hashFundingTx);
        READWRITE(nLockTime);
        READWRITE(vchGameState);
    )

    uint256 GetHash() const
    {
        CDataStream ss;
        ss << hashChallenge << vchPubKeyA << vchPubKeyB;
        return Hash(ss.begin(), ss.end());
    }

    bool IsMyGame() const
    {
        return (mapKeys.count(vchPubKeyA) || mapKeys.count(vchPubKeyB));
    }

    bool AmPlayerA() const
    {
        return mapKeys.count(vchPubKeyA) > 0;
    }
};

// Game database
class CGameDB : public CDB
{
public:
    CGameDB(const char* pszMode="r+", bool fTxn=false) : CDB("game.dat", pszMode, fTxn) { }
private:
    CGameDB(const CGameDB&);
    void operator=(const CGameDB&);
public:
    bool WriteSession(uint256 hash, const CGameSession& session)
    {
        return Write(make_pair(string("session"), hash), session);
    }

    bool ReadSession(uint256 hash, CGameSession& session)
    {
        return Read(make_pair(string("session"), hash), session);
    }
};


// Globals
extern map<uint256, CGameChallenge> mapGameChallenges;
extern map<uint256, CGameSession> mapGameSessions;
extern CCriticalSection cs_mapGames;

// Functions
CScript CreateMultisigScript(const vector<unsigned char>& pubkeyA, const vector<unsigned char>& pubkeyB);
bool AcceptGameChallenge(const CGameChallenge& challenge);
bool AcceptGameAccept(const CGameAccept& accept);
bool ProcessGameMove(const CGameMove& move);
bool ProcessGameSettle(const CGameSettle& settle);
bool SendGameChallenge(int nGameType, int64 nBetAmount);
bool SendGameAccept(const uint256& hashChallenge, CNode* pnodeChallenger);
bool SendGameMove(const uint256& hashSession, const string& strMove, const vector<unsigned char>& vchPayload);
bool CreateFundingTransaction(CGameSession& session, CWalletTx& wtxFunding);
bool CreateRefundTransaction(const CGameSession& session, CTransaction& txRefund, bool fPlayerA);
bool CreateSettlementTransaction(const CGameSession& session, CTransaction& txSettle, const vector<unsigned char>& vchPubKeyWinner);

#endif
