// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef CHESS_H
#define CHESS_H

//
// Piece encoding: bits 0-2 = type, bit 3 = color
//
enum
{
    PIECE_EMPTY  = 0,
    PIECE_PAWN   = 1,
    PIECE_KNIGHT = 2,
    PIECE_BISHOP = 3,
    PIECE_ROOK   = 4,
    PIECE_QUEEN  = 5,
    PIECE_KING   = 6,

    COLOR_WHITE  = 0,
    COLOR_BLACK  = 8,

    PIECE_TYPE_MASK  = 7,
    PIECE_COLOR_MASK = 8,
};

inline int PieceType(unsigned char p)  { return p & PIECE_TYPE_MASK; }
inline int PieceColor(unsigned char p) { return p & PIECE_COLOR_MASK; }
inline bool IsWhite(unsigned char p)   { return p != PIECE_EMPTY && PieceColor(p) == COLOR_WHITE; }
inline bool IsBlack(unsigned char p)   { return p != PIECE_EMPTY && PieceColor(p) == COLOR_BLACK; }


//
// CChessBoard - full chess position with move validation
//
// Square indexing: a1=0, b1=1 ... h1=7, a2=8 ... h8=63
//
class CChessBoard
{
public:
    unsigned char board[64];
    bool fWhiteToMove;
    bool fCastleWK;
    bool fCastleWQ;
    bool fCastleBK;
    bool fCastleBQ;
    int nEnPassantSquare;   // -1 if none
    int nHalfMoveClock;
    int nFullMoveNumber;

    CChessBoard()
    {
        SetInitialPosition();
    }

    void SetInitialPosition();

    // Square conversion helpers
    static int SquareFromString(const string& sq);
    static string SquareToString(int sq);
    static int FileOf(int sq) { return sq % 8; }
    static int RankOf(int sq) { return sq / 8; }
    static int MakeSquare(int file, int rank) { return rank * 8 + file; }

    // Attack detection
    bool IsSquareAttacked(int sq, bool byWhite) const;

    // Check / mate / draw detection
    bool IsCheck() const;
    bool IsCheckmate();
    bool IsStalemate();
    bool IsDraw();

    // Move validation and execution
    bool IsValidMove(const string& strMove);
    bool MakeMove(const string& strMove);

    // Legal move generation
    vector<string> GetLegalMoves();

    // ASCII board for TUI display
    string ToASCII() const;

private:
    bool IsLegalMoveInternal(int from, int to, int promoType) const;
    void DoMove(int from, int to, int promoType);
    int FindKing(bool white) const;
};


//
// CChessGame - manages a full game session
//
class CChessGame
{
public:
    CChessBoard board;
    bool fPlayerIsWhite;
    int nResult;            // 0=ongoing, 1=white wins, 2=black wins, 3=draw
    vector<string> vMoveHistory;

    CChessGame()
    {
        fPlayerIsWhite = true;
        nResult = 0;
    }
};

#endif
