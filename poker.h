// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef POKER_H
#define POKER_H

// Card: 0-51. Suit = card/13 (0=Clubs,1=Diamonds,2=Hearts,3=Spades), Rank = card%13 (0=2, 1=3, ..., 12=Ace)
class CPokerCard
{
public:
    int nCard; // 0-51

    CPokerCard() : nCard(0) {}
    CPokerCard(int n) : nCard(n) {}

    int GetSuit() const { return nCard / 13; }
    int GetRank() const { return nCard % 13; }

    string ToString() const;  // e.g. "[Ah]", "[Ks]", "[7d]", "[3c]"

    IMPLEMENT_SERIALIZE(READWRITE(nCard);)
};

// Hand rankings
enum PokerHandRank
{
    HAND_HIGH_CARD = 0,
    HAND_ONE_PAIR,
    HAND_TWO_PAIR,
    HAND_THREE_OF_A_KIND,
    HAND_STRAIGHT,
    HAND_FLUSH,
    HAND_FULL_HOUSE,
    HAND_FOUR_OF_A_KIND,
    HAND_STRAIGHT_FLUSH,
    HAND_ROYAL_FLUSH,
};

// Poker phases
enum PokerPhase
{
    POKER_ANTE = 0,
    POKER_COMMIT,       // both commit Hash(seed)
    POKER_REVEAL,       // both reveal seed
    POKER_DEAL,         // deck computed, cards dealt
    POKER_BET1,         // first betting round
    POKER_DRAW,         // discard and draw
    POKER_BET2,         // second betting round
    POKER_SHOWDOWN,     // reveal hands
    POKER_DONE,
};

// Actions
enum PokerAction
{
    PACT_FOLD = 0,
    PACT_CHECK,
    PACT_CALL,
    PACT_RAISE,
    PACT_DISCARD,       // "discard 0,2,4"
    PACT_COMMIT_SEED,
    PACT_REVEAL_SEED,
    PACT_SHOW_HAND,
};

class CPokerHand
{
public:
    CPokerCard cards[5];
    int nCount;

    CPokerHand() : nCount(0) {}

    PokerHandRank GetRank() const;
    uint32_t GetScore() const; // comparable score: bits 28-31=rank, rest=kickers
    string ToString() const;
    string RankName() const;
};

class CPokerGame
{
public:
    int nPhase;          // PokerPhase
    int64 nPot;
    int64 nCurrentBet;
    int nDealerSeat;     // 0=A deals, 1=B deals

    // Seeds for commit-reveal dealing
    uint256 hashSeedA;   // commitment Hash(seedA)
    uint256 hashSeedB;
    uint256 seedA;       // revealed seeds
    uint256 seedB;
    bool fSeedARevealed;
    bool fSeedBRevealed;

    // Deck and hands
    CPokerCard deck[52];
    CPokerHand handA;
    CPokerHand handB;
    bool vDiscardA[5];   // cards player A wants to discard
    bool vDiscardB[5];
    int nDrawCardIndex;  // next card to deal from deck

    // Action tracking
    bool fPlayerAFolded;
    bool fPlayerBFolded;
    bool fPlayerAActed;
    bool fPlayerBActed;

    CPokerGame();

    void ShuffleDeck();  // Fisher-Yates with Hash(seedA||seedB)
    void DealCards();     // deal 5 to each
    void DrawCards(bool fPlayerA, const vector<int>& vDiscard);

    bool ProcessAction(bool fPlayerA, PokerAction action, int64 nAmount = 0, const string& strExtra = "");

    int DetermineWinner(); // 0=draw, 1=A wins, 2=B wins

    string GetPhaseString() const;
};

// Hand evaluation
PokerHandRank EvaluateHand(const CPokerCard cards[5]);
uint32_t ScoreHand(const CPokerCard cards[5]);

#endif
