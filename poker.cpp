// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "poker.h"



//////////////////////////////////////////////////////////////////////////////
//
// CPokerCard
//

string CPokerCard::ToString() const
{
    static const char* pszRanks = "23456789TJQKA";
    static const char* pszSuits = "cdhs";

    if (nCard < 0 || nCard > 51)
        return "[??]";

    char sz[5];
    sz[0] = '[';
    sz[1] = pszRanks[GetRank()];
    sz[2] = pszSuits[GetSuit()];
    sz[3] = ']';
    sz[4] = 0;
    return string(sz);
}



//////////////////////////////////////////////////////////////////////////////
//
// Hand evaluation helpers
//

// Sort ranks in descending order for kicker comparison
static void SortRanksDescending(int* pRanks, int nCount)
{
    for (int i = 0; i < nCount - 1; i++)
        for (int j = i + 1; j < nCount; j++)
            if (pRanks[j] > pRanks[i])
            {
                int tmp = pRanks[i];
                pRanks[i] = pRanks[j];
                pRanks[j] = tmp;
            }
}

PokerHandRank EvaluateHand(const CPokerCard cards[5])
{
    // Count ranks and suits
    int nRankCount[13];
    int nSuitCount[4];
    memset(nRankCount, 0, sizeof(nRankCount));
    memset(nSuitCount, 0, sizeof(nSuitCount));

    for (int i = 0; i < 5; i++)
    {
        nRankCount[cards[i].GetRank()]++;
        nSuitCount[cards[i].GetSuit()]++;
    }

    // Check flush
    bool fFlush = false;
    for (int i = 0; i < 4; i++)
        if (nSuitCount[i] == 5)
            fFlush = true;

    // Check straight
    bool fStraight = false;
    int nHighCard = -1;
    for (int i = 0; i <= 8; i++) // 2-6 through T-A
    {
        if (nRankCount[i] && nRankCount[i+1] && nRankCount[i+2] && nRankCount[i+3] && nRankCount[i+4])
        {
            fStraight = true;
            nHighCard = i + 4;
            break;
        }
    }
    // Check A-2-3-4-5 (wheel)
    if (nRankCount[12] && nRankCount[0] && nRankCount[1] && nRankCount[2] && nRankCount[3])
    {
        fStraight = true;
        nHighCard = 3; // 5 is high in a wheel
    }

    // Count pairs, trips, quads
    int nPairs = 0;
    int nTrips = 0;
    int nQuads = 0;
    for (int i = 0; i < 13; i++)
    {
        if (nRankCount[i] == 2) nPairs++;
        if (nRankCount[i] == 3) nTrips++;
        if (nRankCount[i] == 4) nQuads++;
    }

    // Royal flush
    if (fFlush && fStraight && nHighCard == 12)
        return HAND_ROYAL_FLUSH;

    // Straight flush
    if (fFlush && fStraight)
        return HAND_STRAIGHT_FLUSH;

    // Four of a kind
    if (nQuads == 1)
        return HAND_FOUR_OF_A_KIND;

    // Full house
    if (nTrips == 1 && nPairs == 1)
        return HAND_FULL_HOUSE;

    // Flush
    if (fFlush)
        return HAND_FLUSH;

    // Straight
    if (fStraight)
        return HAND_STRAIGHT;

    // Three of a kind
    if (nTrips == 1)
        return HAND_THREE_OF_A_KIND;

    // Two pair
    if (nPairs == 2)
        return HAND_TWO_PAIR;

    // One pair
    if (nPairs == 1)
        return HAND_ONE_PAIR;

    return HAND_HIGH_CARD;
}


//
// ScoreHand - returns a 32-bit comparable value
//
// Bits 28-31: hand rank (0-9)
// Bits 0-27: kicker values packed in descending order (4 bits each, up to 7 nibbles)
//
// Higher score = better hand. Hands can be compared with simple integer comparison.
//
uint32_t ScoreHand(const CPokerCard cards[5])
{
    PokerHandRank rank = EvaluateHand(cards);
    uint32_t nScore = ((uint32_t)rank) << 28;

    int nRankCount[13];
    memset(nRankCount, 0, sizeof(nRankCount));
    for (int i = 0; i < 5; i++)
        nRankCount[cards[i].GetRank()]++;

    // Build sorted kicker list based on hand type
    int vKickers[5];
    int nKickers = 0;

    if (rank == HAND_FOUR_OF_A_KIND)
    {
        // Quad rank first, then kicker
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 4)
                vKickers[nKickers++] = i;
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 1)
                vKickers[nKickers++] = i;
    }
    else if (rank == HAND_FULL_HOUSE)
    {
        // Trips rank first, then pair rank
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 3)
                vKickers[nKickers++] = i;
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 2)
                vKickers[nKickers++] = i;
    }
    else if (rank == HAND_THREE_OF_A_KIND)
    {
        // Trips rank, then remaining descending
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 3)
                vKickers[nKickers++] = i;
        int vSingles[2];
        int nSingles = 0;
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 1)
                vSingles[nSingles++] = i;
        for (int i = 0; i < nSingles; i++)
            vKickers[nKickers++] = vSingles[i];
    }
    else if (rank == HAND_TWO_PAIR)
    {
        // Higher pair, lower pair, kicker
        int vPairs[2];
        int nPairCount = 0;
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 2)
                vPairs[nPairCount++] = i;
        vKickers[nKickers++] = vPairs[0];
        vKickers[nKickers++] = vPairs[1];
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 1)
                vKickers[nKickers++] = i;
    }
    else if (rank == HAND_ONE_PAIR)
    {
        // Pair rank, then remaining descending
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 2)
                vKickers[nKickers++] = i;
        for (int i = 12; i >= 0; i--)
            if (nRankCount[i] == 1)
                vKickers[nKickers++] = i;
    }
    else if (rank == HAND_STRAIGHT || rank == HAND_STRAIGHT_FLUSH || rank == HAND_ROYAL_FLUSH)
    {
        // High card of the straight
        // Check for wheel (A-2-3-4-5)
        if (nRankCount[12] && nRankCount[0] && nRankCount[1] && nRankCount[2] && nRankCount[3])
            vKickers[nKickers++] = 3; // 5 is high card in wheel
        else
        {
            for (int i = 12; i >= 0; i--)
                if (nRankCount[i])
                {
                    vKickers[nKickers++] = i;
                    break;
                }
        }
    }
    else
    {
        // High card or flush: all cards descending
        for (int i = 12; i >= 0; i--)
            for (int j = 0; j < nRankCount[i]; j++)
                vKickers[nKickers++] = i;
    }

    // Pack kickers into lower 28 bits (4 bits each)
    for (int i = 0; i < nKickers && i < 7; i++)
        nScore |= ((uint32_t)vKickers[i]) << (24 - i * 4);

    return nScore;
}



//////////////////////////////////////////////////////////////////////////////
//
// CPokerHand
//

PokerHandRank CPokerHand::GetRank() const
{
    return EvaluateHand(cards);
}

uint32_t CPokerHand::GetScore() const
{
    return ScoreHand(cards);
}

string CPokerHand::RankName() const
{
    switch (GetRank())
    {
    case HAND_HIGH_CARD:       return "High Card";
    case HAND_ONE_PAIR:        return "One Pair";
    case HAND_TWO_PAIR:        return "Two Pair";
    case HAND_THREE_OF_A_KIND: return "Three of a Kind";
    case HAND_STRAIGHT:        return "Straight";
    case HAND_FLUSH:           return "Flush";
    case HAND_FULL_HOUSE:      return "Full House";
    case HAND_FOUR_OF_A_KIND:  return "Four of a Kind";
    case HAND_STRAIGHT_FLUSH:  return "Straight Flush";
    case HAND_ROYAL_FLUSH:     return "Royal Flush";
    }
    return "Unknown";
}

string CPokerHand::ToString() const
{
    string str;
    for (int i = 0; i < nCount; i++)
    {
        if (i > 0) str += " ";
        str += cards[i].ToString();
    }
    str += "  (" + RankName() + ")";
    return str;
}



//////////////////////////////////////////////////////////////////////////////
//
// CPokerGame
//

CPokerGame::CPokerGame()
{
    nPhase = POKER_ANTE;
    nPot = 0;
    nCurrentBet = 0;
    nDealerSeat = 0;

    hashSeedA = 0;
    hashSeedB = 0;
    seedA = 0;
    seedB = 0;
    fSeedARevealed = false;
    fSeedBRevealed = false;

    for (int i = 0; i < 52; i++)
        deck[i] = CPokerCard(i);

    memset(vDiscardA, 0, sizeof(vDiscardA));
    memset(vDiscardB, 0, sizeof(vDiscardB));
    nDrawCardIndex = 10; // first 10 cards dealt to players

    fPlayerAFolded = false;
    fPlayerBFolded = false;
    fPlayerAActed = false;
    fPlayerBActed = false;
}

string CPokerGame::GetPhaseString() const
{
    switch (nPhase)
    {
    case POKER_ANTE:     return "Ante";
    case POKER_COMMIT:   return "Commit Seeds";
    case POKER_REVEAL:   return "Reveal Seeds";
    case POKER_DEAL:     return "Deal";
    case POKER_BET1:     return "Betting Round 1";
    case POKER_DRAW:     return "Draw";
    case POKER_BET2:     return "Betting Round 2";
    case POKER_SHOWDOWN: return "Showdown";
    case POKER_DONE:     return "Done";
    }
    return "Unknown";
}


//
// ShuffleDeck - deterministic Fisher-Yates using Hash(seedA||seedB) as PRNG
//
void CPokerGame::ShuffleDeck()
{
    // Initialize deck in order
    for (int i = 0; i < 52; i++)
        deck[i] = CPokerCard(i);

    // Combine seeds: Hash(seedA || seedB) gives initial entropy
    uint256 hashEntropy = Hash(seedA.begin(), seedA.end(),
                               seedB.begin(), seedB.end());

    // Fisher-Yates shuffle
    for (int i = 51; i > 0; i--)
    {
        // Use hash bytes to generate a random index
        // Re-hash when we exhaust bytes
        if (i % 8 == 0)
        {
            hashEntropy = Hash(hashEntropy.begin(), hashEntropy.end());
        }

        // Extract 32-bit value from hash and compute index
        int nOffset = (i % 8) * 4;
        uint32_t nRandom = 0;
        memcpy(&nRandom, hashEntropy.begin() + nOffset, 4);
        int j = nRandom % (i + 1);

        // Swap
        CPokerCard tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
}


//
// DealCards - deal 5 cards to each player from shuffled deck
//
void CPokerGame::DealCards()
{
    // Cards 0-4 to player A, 5-9 to player B
    for (int i = 0; i < 5; i++)
    {
        handA.cards[i] = deck[i];
        handB.cards[i] = deck[i + 5];
    }
    handA.nCount = 5;
    handB.nCount = 5;
    nDrawCardIndex = 10;

    printf("Poker: dealt cards to both players\n");
}


//
// DrawCards - replace discarded cards with new ones from the deck
//
void CPokerGame::DrawCards(bool fPlayerA, const vector<int>& vDiscard)
{
    CPokerHand& hand = fPlayerA ? handA : handB;
    bool* vDiscardFlags = fPlayerA ? vDiscardA : vDiscardB;

    // Mark discards
    memset(vDiscardFlags, 0, 5 * sizeof(bool));
    for (unsigned int i = 0; i < vDiscard.size(); i++)
    {
        int idx = vDiscard[i];
        if (idx >= 0 && idx < 5)
            vDiscardFlags[idx] = true;
    }

    // Replace discarded cards from the deck
    int nDrawn = 0;
    for (int i = 0; i < 5; i++)
    {
        if (vDiscardFlags[i])
        {
            if (nDrawCardIndex < 52)
            {
                hand.cards[i] = deck[nDrawCardIndex];
                nDrawCardIndex++;
                nDrawn++;
            }
        }
    }

    printf("Poker: player %c drew %d cards\n", fPlayerA ? 'A' : 'B', nDrawn);
}


//
// ProcessAction - handle a player action, advance phases as needed
//
bool CPokerGame::ProcessAction(bool fPlayerA, PokerAction action, int64 nAmount, const string& strExtra)
{
    switch (action)
    {
    case PACT_COMMIT_SEED:
    {
        if (nPhase != POKER_COMMIT)
            return false;

        // strExtra contains the hex-encoded hash commitment
        uint256 hashCommit;
        hashCommit.SetHex(strExtra);
        if (hashCommit == 0)
            return false;

        if (fPlayerA)
            hashSeedA = hashCommit;
        else
            hashSeedB = hashCommit;

        printf("Poker: player %c committed seed hash\n", fPlayerA ? 'A' : 'B');

        // If both have committed, advance to reveal phase
        if (hashSeedA != 0 && hashSeedB != 0)
        {
            nPhase = POKER_REVEAL;
            printf("Poker: both seeds committed, entering reveal phase\n");
        }
        return true;
    }

    case PACT_REVEAL_SEED:
    {
        if (nPhase != POKER_REVEAL)
            return false;

        // strExtra contains the hex-encoded seed
        uint256 seed;
        seed.SetHex(strExtra);

        // Verify the revealed seed matches the commitment
        uint256 hashCheck = Hash(seed.begin(), seed.end());

        if (fPlayerA)
        {
            if (hashCheck != hashSeedA)
            {
                printf("Poker: player A seed does not match commitment!\n");
                return false;
            }
            seedA = seed;
            fSeedARevealed = true;
        }
        else
        {
            if (hashCheck != hashSeedB)
            {
                printf("Poker: player B seed does not match commitment!\n");
                return false;
            }
            seedB = seed;
            fSeedBRevealed = true;
        }

        printf("Poker: player %c revealed seed\n", fPlayerA ? 'A' : 'B');

        // If both revealed, shuffle and deal
        if (fSeedARevealed && fSeedBRevealed)
        {
            ShuffleDeck();
            DealCards();
            nPhase = POKER_BET1;
            fPlayerAActed = false;
            fPlayerBActed = false;
            printf("Poker: deck shuffled, cards dealt, entering first betting round\n");
        }
        return true;
    }

    case PACT_FOLD:
    {
        if (nPhase != POKER_BET1 && nPhase != POKER_BET2)
            return false;

        if (fPlayerA)
            fPlayerAFolded = true;
        else
            fPlayerBFolded = true;

        printf("Poker: player %c folded\n", fPlayerA ? 'A' : 'B');
        nPhase = POKER_DONE;
        return true;
    }

    case PACT_CHECK:
    {
        if (nPhase != POKER_BET1 && nPhase != POKER_BET2)
            return false;
        if (nCurrentBet > 0)
            return false; // can't check when there's a bet to call

        if (fPlayerA)
            fPlayerAActed = true;
        else
            fPlayerBActed = true;

        printf("Poker: player %c checked\n", fPlayerA ? 'A' : 'B');

        // If both have acted, advance phase
        if (fPlayerAActed && fPlayerBActed)
        {
            if (nPhase == POKER_BET1)
            {
                nPhase = POKER_DRAW;
                fPlayerAActed = false;
                fPlayerBActed = false;
                printf("Poker: entering draw phase\n");
            }
            else
            {
                nPhase = POKER_SHOWDOWN;
                printf("Poker: entering showdown\n");
            }
        }
        return true;
    }

    case PACT_CALL:
    {
        if (nPhase != POKER_BET1 && nPhase != POKER_BET2)
            return false;
        if (nCurrentBet <= 0)
            return false; // nothing to call

        nPot += nCurrentBet;

        if (fPlayerA)
            fPlayerAActed = true;
        else
            fPlayerBActed = true;

        printf("Poker: player %c called %" PRId64 "\n", fPlayerA ? 'A' : 'B', nCurrentBet);

        nCurrentBet = 0;

        // If both have acted, advance phase
        if (fPlayerAActed && fPlayerBActed)
        {
            if (nPhase == POKER_BET1)
            {
                nPhase = POKER_DRAW;
                fPlayerAActed = false;
                fPlayerBActed = false;
                printf("Poker: entering draw phase\n");
            }
            else
            {
                nPhase = POKER_SHOWDOWN;
                printf("Poker: entering showdown\n");
            }
        }
        return true;
    }

    case PACT_RAISE:
    {
        if (nPhase != POKER_BET1 && nPhase != POKER_BET2)
            return false;
        if (nAmount <= 0)
            return false;

        // Match current bet plus raise
        nPot += nCurrentBet + nAmount;
        nCurrentBet = nAmount;

        // Raiser has acted, other player needs to respond
        if (fPlayerA)
        {
            fPlayerAActed = true;
            fPlayerBActed = false;
        }
        else
        {
            fPlayerBActed = true;
            fPlayerAActed = false;
        }

        printf("Poker: player %c raised %" PRId64 "\n", fPlayerA ? 'A' : 'B', nAmount);
        return true;
    }

    case PACT_DISCARD:
    {
        if (nPhase != POKER_DRAW)
            return false;

        // Parse discard indices from strExtra (comma-separated: "0,2,4")
        vector<int> vDiscard;
        if (!strExtra.empty())
        {
            string strParse = strExtra;
            for (unsigned int i = 0; i < strParse.size(); i++)
                if (strParse[i] == ',')
                    strParse[i] = ' ';

            istringstream iss(strParse);
            int idx;
            while (iss >> idx)
            {
                if (idx >= 0 && idx < 5)
                    vDiscard.push_back(idx);
            }
        }

        DrawCards(fPlayerA, vDiscard);

        if (fPlayerA)
            fPlayerAActed = true;
        else
            fPlayerBActed = true;

        // If both have drawn, advance to second betting round
        if (fPlayerAActed && fPlayerBActed)
        {
            nPhase = POKER_BET2;
            nCurrentBet = 0;
            fPlayerAActed = false;
            fPlayerBActed = false;
            printf("Poker: draw complete, entering second betting round\n");
        }
        return true;
    }

    case PACT_SHOW_HAND:
    {
        if (nPhase != POKER_SHOWDOWN)
            return false;

        if (fPlayerA)
            fPlayerAActed = true;
        else
            fPlayerBActed = true;

        printf("Poker: player %c shows hand: %s\n", fPlayerA ? 'A' : 'B',
               (fPlayerA ? handA : handB).ToString().c_str());

        // If both have shown, determine winner
        if (fPlayerAActed && fPlayerBActed)
        {
            int nWinner = DetermineWinner();
            nPhase = POKER_DONE;

            if (nWinner == 0)
                printf("Poker: it's a draw! pot split\n");
            else
                printf("Poker: player %c wins the pot of %" PRId64 "\n",
                       nWinner == 1 ? 'A' : 'B', nPot);
        }
        return true;
    }

    default:
        return false;
    }

    return false;
}


//
// DetermineWinner - compare hand scores
//
// Returns: 0=draw, 1=A wins, 2=B wins
//
int CPokerGame::DetermineWinner()
{
    // If someone folded, the other player wins
    if (fPlayerAFolded)
        return 2;
    if (fPlayerBFolded)
        return 1;

    uint32_t nScoreA = handA.GetScore();
    uint32_t nScoreB = handB.GetScore();

    printf("Poker: hand A: %s (score %08x)\n", handA.ToString().c_str(), nScoreA);
    printf("Poker: hand B: %s (score %08x)\n", handB.ToString().c_str(), nScoreB);

    if (nScoreA > nScoreB)
        return 1;
    if (nScoreB > nScoreA)
        return 2;
    return 0;
}
