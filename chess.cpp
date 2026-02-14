// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "chess.h"




//
// SetInitialPosition - standard starting position
//
void CChessBoard::SetInitialPosition()
{
    memset(board, PIECE_EMPTY, sizeof(board));

    // White pieces (rank 1)
    board[0] = COLOR_WHITE | PIECE_ROOK;
    board[1] = COLOR_WHITE | PIECE_KNIGHT;
    board[2] = COLOR_WHITE | PIECE_BISHOP;
    board[3] = COLOR_WHITE | PIECE_QUEEN;
    board[4] = COLOR_WHITE | PIECE_KING;
    board[5] = COLOR_WHITE | PIECE_BISHOP;
    board[6] = COLOR_WHITE | PIECE_KNIGHT;
    board[7] = COLOR_WHITE | PIECE_ROOK;

    // White pawns (rank 2)
    for (int i = 8; i < 16; i++)
        board[i] = COLOR_WHITE | PIECE_PAWN;

    // Black pawns (rank 7)
    for (int i = 48; i < 56; i++)
        board[i] = COLOR_BLACK | PIECE_PAWN;

    // Black pieces (rank 8)
    board[56] = COLOR_BLACK | PIECE_ROOK;
    board[57] = COLOR_BLACK | PIECE_KNIGHT;
    board[58] = COLOR_BLACK | PIECE_BISHOP;
    board[59] = COLOR_BLACK | PIECE_QUEEN;
    board[60] = COLOR_BLACK | PIECE_KING;
    board[61] = COLOR_BLACK | PIECE_BISHOP;
    board[62] = COLOR_BLACK | PIECE_KNIGHT;
    board[63] = COLOR_BLACK | PIECE_ROOK;

    fWhiteToMove = true;
    fCastleWK = true;
    fCastleWQ = true;
    fCastleBK = true;
    fCastleBQ = true;
    nEnPassantSquare = -1;
    nHalfMoveClock = 0;
    nFullMoveNumber = 1;
}



//
// Square conversion helpers
//
int CChessBoard::SquareFromString(const string& sq)
{
    if (sq.size() < 2)
        return -1;
    int file = sq[0] - 'a';
    int rank = sq[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;
    return MakeSquare(file, rank);
}

string CChessBoard::SquareToString(int sq)
{
    if (sq < 0 || sq > 63)
        return "??";
    string s;
    s += (char)('a' + FileOf(sq));
    s += (char)('1' + RankOf(sq));
    return s;
}



//
// FindKing - locate king square for the given side
//
int CChessBoard::FindKing(bool white) const
{
    unsigned char target = (white ? COLOR_WHITE : COLOR_BLACK) | PIECE_KING;
    for (int i = 0; i < 64; i++)
        if (board[i] == target)
            return i;
    return -1;
}



//
// IsSquareAttacked - check if a square is attacked by the given side
//
bool CChessBoard::IsSquareAttacked(int sq, bool byWhite) const
{
    int color = byWhite ? COLOR_WHITE : COLOR_BLACK;

    // Knight attacks
    static const int knightOffsets[8] = { -17, -15, -10, -6, 6, 10, 15, 17 };
    for (int i = 0; i < 8; i++)
    {
        int from = sq + knightOffsets[i];
        if (from < 0 || from > 63)
            continue;
        // Verify the knight move doesn't wrap around the board
        int df = abs(FileOf(from) - FileOf(sq));
        int dr = abs(RankOf(from) - RankOf(sq));
        if (!((df == 1 && dr == 2) || (df == 2 && dr == 1)))
            continue;
        if (board[from] == (unsigned char)(color | PIECE_KNIGHT))
            return true;
    }

    // Pawn attacks
    if (byWhite)
    {
        // White pawn attacks upward, so check if a white pawn is below-left or below-right
        int r = RankOf(sq), f = FileOf(sq);
        if (r > 0)
        {
            if (f > 0 && board[MakeSquare(f - 1, r - 1)] == (unsigned char)(COLOR_WHITE | PIECE_PAWN))
                return true;
            if (f < 7 && board[MakeSquare(f + 1, r - 1)] == (unsigned char)(COLOR_WHITE | PIECE_PAWN))
                return true;
        }
    }
    else
    {
        // Black pawn attacks downward
        int r = RankOf(sq), f = FileOf(sq);
        if (r < 7)
        {
            if (f > 0 && board[MakeSquare(f - 1, r + 1)] == (unsigned char)(COLOR_BLACK | PIECE_PAWN))
                return true;
            if (f < 7 && board[MakeSquare(f + 1, r + 1)] == (unsigned char)(COLOR_BLACK | PIECE_PAWN))
                return true;
        }
    }

    // King attacks (adjacent squares)
    for (int dr = -1; dr <= 1; dr++)
    {
        for (int df = -1; df <= 1; df++)
        {
            if (dr == 0 && df == 0)
                continue;
            int r = RankOf(sq) + dr;
            int f = FileOf(sq) + df;
            if (r < 0 || r > 7 || f < 0 || f > 7)
                continue;
            if (board[MakeSquare(f, r)] == (unsigned char)(color | PIECE_KING))
                return true;
        }
    }

    // Sliding pieces: rook/queen along ranks and files
    static const int rookDirs[4][2] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
    for (int d = 0; d < 4; d++)
    {
        int f = FileOf(sq) + rookDirs[d][0];
        int r = RankOf(sq) + rookDirs[d][1];
        while (f >= 0 && f <= 7 && r >= 0 && r <= 7)
        {
            unsigned char p = board[MakeSquare(f, r)];
            if (p != PIECE_EMPTY)
            {
                if (PieceColor(p) == color && (PieceType(p) == PIECE_ROOK || PieceType(p) == PIECE_QUEEN))
                    return true;
                break;
            }
            f += rookDirs[d][0];
            r += rookDirs[d][1];
        }
    }

    // Sliding pieces: bishop/queen along diagonals
    static const int bishopDirs[4][2] = { {1,1}, {1,-1}, {-1,1}, {-1,-1} };
    for (int d = 0; d < 4; d++)
    {
        int f = FileOf(sq) + bishopDirs[d][0];
        int r = RankOf(sq) + bishopDirs[d][1];
        while (f >= 0 && f <= 7 && r >= 0 && r <= 7)
        {
            unsigned char p = board[MakeSquare(f, r)];
            if (p != PIECE_EMPTY)
            {
                if (PieceColor(p) == color && (PieceType(p) == PIECE_BISHOP || PieceType(p) == PIECE_QUEEN))
                    return true;
                break;
            }
            f += bishopDirs[d][0];
            r += bishopDirs[d][1];
        }
    }

    return false;
}



//
// IsCheck - is the current side's king in check?
//
bool CChessBoard::IsCheck() const
{
    int kingSq = FindKing(fWhiteToMove);
    if (kingSq < 0)
        return false;
    return IsSquareAttacked(kingSq, !fWhiteToMove);
}



//
// IsLegalMoveInternal - test a pseudo-legal move for full legality
//
// Makes the move on a copy of the board and checks that the moving side's
// king is not left in check.  Also validates basic piece movement rules,
// castling conditions, en passant, and promotion.
//
bool CChessBoard::IsLegalMoveInternal(int from, int to, int promoType) const
{
    if (from < 0 || from > 63 || to < 0 || to > 63 || from == to)
        return false;

    unsigned char piece = board[from];
    if (piece == PIECE_EMPTY)
        return false;

    bool white = IsWhite(piece);
    if (white != fWhiteToMove)
        return false;

    unsigned char target = board[to];
    // Can't capture own piece
    if (target != PIECE_EMPTY && IsWhite(target) == white)
        return false;

    int type = PieceType(piece);
    int ff = FileOf(from), fr = RankOf(from);
    int tf = FileOf(to),   tr = RankOf(to);
    int df = tf - ff, dr = tr - fr;

    switch (type)
    {
    case PIECE_PAWN:
    {
        int dir = white ? 1 : -1;
        int startRank = white ? 1 : 6;
        int promoRank = white ? 7 : 0;

        if (df == 0)
        {
            // Forward move
            if (dr == dir && target == PIECE_EMPTY)
            {
                // ok - single push
            }
            else if (dr == 2 * dir && fr == startRank && target == PIECE_EMPTY &&
                     board[MakeSquare(ff, fr + dir)] == PIECE_EMPTY)
            {
                // ok - double push
            }
            else
            {
                return false;
            }
        }
        else if (abs(df) == 1 && dr == dir)
        {
            // Diagonal capture or en passant
            if (target != PIECE_EMPTY)
            {
                // normal capture
            }
            else if (to == nEnPassantSquare)
            {
                // en passant
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        // Promotion validation
        if (tr == promoRank)
        {
            if (promoType != PIECE_QUEEN && promoType != PIECE_ROOK &&
                promoType != PIECE_BISHOP && promoType != PIECE_KNIGHT)
                return false;
        }
        else
        {
            if (promoType != PIECE_EMPTY)
                return false;
        }
        break;
    }

    case PIECE_KNIGHT:
    {
        int adf = abs(df), adr = abs(dr);
        if (!((adf == 1 && adr == 2) || (adf == 2 && adr == 1)))
            return false;
        break;
    }

    case PIECE_BISHOP:
    {
        if (abs(df) != abs(dr) || df == 0)
            return false;
        // Check path is clear
        int sf = (df > 0) ? 1 : -1;
        int sr = (dr > 0) ? 1 : -1;
        int cf = ff + sf, cr = fr + sr;
        while (cf != tf || cr != tr)
        {
            if (board[MakeSquare(cf, cr)] != PIECE_EMPTY)
                return false;
            cf += sf;
            cr += sr;
        }
        break;
    }

    case PIECE_ROOK:
    {
        if (df != 0 && dr != 0)
            return false;
        // Check path is clear
        int sf = (df == 0) ? 0 : ((df > 0) ? 1 : -1);
        int sr = (dr == 0) ? 0 : ((dr > 0) ? 1 : -1);
        int cf = ff + sf, cr = fr + sr;
        while (cf != tf || cr != tr)
        {
            if (board[MakeSquare(cf, cr)] != PIECE_EMPTY)
                return false;
            cf += sf;
            cr += sr;
        }
        break;
    }

    case PIECE_QUEEN:
    {
        bool isDiag = (abs(df) == abs(dr) && df != 0);
        bool isStraight = (df == 0 || dr == 0);
        if (!isDiag && !isStraight)
            return false;
        if (df == 0 && dr == 0)
            return false;
        int sf = (df == 0) ? 0 : ((df > 0) ? 1 : -1);
        int sr = (dr == 0) ? 0 : ((dr > 0) ? 1 : -1);
        int cf = ff + sf, cr = fr + sr;
        while (cf != tf || cr != tr)
        {
            if (board[MakeSquare(cf, cr)] != PIECE_EMPTY)
                return false;
            cf += sf;
            cr += sr;
        }
        break;
    }

    case PIECE_KING:
    {
        // Castling
        if (abs(df) == 2 && dr == 0)
        {
            if (white)
            {
                if (from != 4 || fr != 0)
                    return false;
                if (df == 2)
                {
                    // Kingside
                    if (!fCastleWK)
                        return false;
                    if (board[5] != PIECE_EMPTY || board[6] != PIECE_EMPTY)
                        return false;
                    if (IsSquareAttacked(4, false) || IsSquareAttacked(5, false) || IsSquareAttacked(6, false))
                        return false;
                }
                else
                {
                    // Queenside
                    if (!fCastleWQ)
                        return false;
                    if (board[3] != PIECE_EMPTY || board[2] != PIECE_EMPTY || board[1] != PIECE_EMPTY)
                        return false;
                    if (IsSquareAttacked(4, false) || IsSquareAttacked(3, false) || IsSquareAttacked(2, false))
                        return false;
                }
            }
            else
            {
                if (from != 60 || fr != 7)
                    return false;
                if (df == 2)
                {
                    // Kingside
                    if (!fCastleBK)
                        return false;
                    if (board[61] != PIECE_EMPTY || board[62] != PIECE_EMPTY)
                        return false;
                    if (IsSquareAttacked(60, true) || IsSquareAttacked(61, true) || IsSquareAttacked(62, true))
                        return false;
                }
                else
                {
                    // Queenside
                    if (!fCastleBQ)
                        return false;
                    if (board[59] != PIECE_EMPTY || board[58] != PIECE_EMPTY || board[57] != PIECE_EMPTY)
                        return false;
                    if (IsSquareAttacked(60, true) || IsSquareAttacked(59, true) || IsSquareAttacked(58, true))
                        return false;
                }
            }
        }
        else
        {
            if (abs(df) > 1 || abs(dr) > 1)
                return false;
        }
        break;
    }

    default:
        return false;
    }

    // Make the move on a temporary copy and verify king is not in check
    CChessBoard tmp = *this;
    tmp.DoMove(from, to, promoType);
    // After the move, it becomes the opponent's turn, but we need to check
    // if OUR king (the side that just moved) is in check
    int kingSq = tmp.FindKing(white);
    if (kingSq < 0)
        return false;
    if (tmp.IsSquareAttacked(kingSq, !white))
        return false;

    return true;
}



//
// DoMove - execute a move on the board (no legality check)
//
void CChessBoard::DoMove(int from, int to, int promoType)
{
    unsigned char piece = board[from];
    int type = PieceType(piece);
    bool white = IsWhite(piece);
    int color = white ? COLOR_WHITE : COLOR_BLACK;

    bool isCapture = (board[to] != PIECE_EMPTY);
    bool isPawnMove = (type == PIECE_PAWN);

    // Handle en passant capture
    if (type == PIECE_PAWN && to == nEnPassantSquare)
    {
        int capturedPawnSq = MakeSquare(FileOf(to), RankOf(from));
        board[capturedPawnSq] = PIECE_EMPTY;
        isCapture = true;
    }

    // Handle castling rook movement
    if (type == PIECE_KING && abs(FileOf(to) - FileOf(from)) == 2)
    {
        if (FileOf(to) == 6) // kingside
        {
            int rookFrom = MakeSquare(7, RankOf(from));
            int rookTo   = MakeSquare(5, RankOf(from));
            board[rookTo] = board[rookFrom];
            board[rookFrom] = PIECE_EMPTY;
        }
        else // queenside
        {
            int rookFrom = MakeSquare(0, RankOf(from));
            int rookTo   = MakeSquare(3, RankOf(from));
            board[rookTo] = board[rookFrom];
            board[rookFrom] = PIECE_EMPTY;
        }
    }

    // Move the piece
    board[to] = piece;
    board[from] = PIECE_EMPTY;

    // Handle promotion
    if (type == PIECE_PAWN && (RankOf(to) == 0 || RankOf(to) == 7))
    {
        if (promoType >= PIECE_KNIGHT && promoType <= PIECE_QUEEN)
            board[to] = (unsigned char)(color | promoType);
        else
            board[to] = (unsigned char)(color | PIECE_QUEEN); // default to queen
    }

    // Update en passant square
    if (type == PIECE_PAWN && abs(RankOf(to) - RankOf(from)) == 2)
        nEnPassantSquare = MakeSquare(FileOf(from), (RankOf(from) + RankOf(to)) / 2);
    else
        nEnPassantSquare = -1;

    // Update castling rights
    if (type == PIECE_KING)
    {
        if (white) { fCastleWK = false; fCastleWQ = false; }
        else       { fCastleBK = false; fCastleBQ = false; }
    }
    if (type == PIECE_ROOK)
    {
        if (from == 0)  fCastleWQ = false;
        if (from == 7)  fCastleWK = false;
        if (from == 56) fCastleBQ = false;
        if (from == 63) fCastleBK = false;
    }
    // Also revoke if a rook is captured on its starting square
    if (to == 0)  fCastleWQ = false;
    if (to == 7)  fCastleWK = false;
    if (to == 56) fCastleBQ = false;
    if (to == 63) fCastleBK = false;

    // Update clocks
    if (isCapture || isPawnMove)
        nHalfMoveClock = 0;
    else
        nHalfMoveClock++;

    if (!white)
        nFullMoveNumber++;

    fWhiteToMove = !fWhiteToMove;
}



//
// Parse a move string like "e2e4" or "e7e8q"
// Returns true and sets from, to, promoType
//
static bool ParseMoveString(const string& strMove, int& from, int& to, int& promoType)
{
    if (strMove.size() < 4 || strMove.size() > 5)
        return false;

    from = CChessBoard::SquareFromString(strMove.substr(0, 2));
    to   = CChessBoard::SquareFromString(strMove.substr(2, 2));
    if (from < 0 || to < 0)
        return false;

    promoType = PIECE_EMPTY;
    if (strMove.size() == 5)
    {
        switch (strMove[4])
        {
        case 'q': case 'Q': promoType = PIECE_QUEEN;  break;
        case 'r': case 'R': promoType = PIECE_ROOK;   break;
        case 'b': case 'B': promoType = PIECE_BISHOP; break;
        case 'n': case 'N': promoType = PIECE_KNIGHT; break;
        default: return false;
        }
    }

    return true;
}



//
// IsValidMove - check if a move string is legal
//
bool CChessBoard::IsValidMove(const string& strMove)
{
    int from, to, promoType;
    if (!ParseMoveString(strMove, from, to, promoType))
        return false;

    // If pawn reaches promotion rank and no promo specified, try queen
    unsigned char piece = board[from];
    if (PieceType(piece) == PIECE_PAWN && promoType == PIECE_EMPTY)
    {
        int promoRank = fWhiteToMove ? 7 : 0;
        if (RankOf(to) == promoRank)
            promoType = PIECE_QUEEN;
    }

    return IsLegalMoveInternal(from, to, promoType);
}



//
// MakeMove - validate and apply a move
//
bool CChessBoard::MakeMove(const string& strMove)
{
    int from, to, promoType;
    if (!ParseMoveString(strMove, from, to, promoType))
        return false;

    // If pawn reaches promotion rank and no promo specified, default to queen
    unsigned char piece = board[from];
    if (PieceType(piece) == PIECE_PAWN && promoType == PIECE_EMPTY)
    {
        int promoRank = fWhiteToMove ? 7 : 0;
        if (RankOf(to) == promoRank)
            promoType = PIECE_QUEEN;
    }

    if (!IsLegalMoveInternal(from, to, promoType))
        return false;

    DoMove(from, to, promoType);
    return true;
}



//
// GetLegalMoves - generate all legal moves for the side to move
//
vector<string> CChessBoard::GetLegalMoves()
{
    vector<string> vMoves;
    int color = fWhiteToMove ? COLOR_WHITE : COLOR_BLACK;

    for (int from = 0; from < 64; from++)
    {
        unsigned char piece = board[from];
        if (piece == PIECE_EMPTY || PieceColor(piece) != color)
            continue;

        int type = PieceType(piece);
        int ff = FileOf(from), fr = RankOf(from);

        switch (type)
        {
        case PIECE_PAWN:
        {
            int dir = fWhiteToMove ? 1 : -1;
            int startRank = fWhiteToMove ? 1 : 6;
            int promoRank = fWhiteToMove ? 7 : 0;

            // Single push
            int to = MakeSquare(ff, fr + dir);
            if (to >= 0 && to < 64 && board[to] == PIECE_EMPTY)
            {
                if (RankOf(to) == promoRank)
                {
                    int promos[] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };
                    for (int i = 0; i < 4; i++)
                    {
                        if (IsLegalMoveInternal(from, to, promos[i]))
                        {
                            string s = SquareToString(from) + SquareToString(to);
                            const char pc[] = { 0, 0, 'n', 'b', 'r', 'q' };
                            s += pc[promos[i]];
                            vMoves.push_back(s);
                        }
                    }
                }
                else
                {
                    if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                        vMoves.push_back(SquareToString(from) + SquareToString(to));
                }

                // Double push
                if (fr == startRank)
                {
                    int to2 = MakeSquare(ff, fr + 2 * dir);
                    if (to2 >= 0 && to2 < 64 && board[to2] == PIECE_EMPTY)
                    {
                        if (IsLegalMoveInternal(from, to2, PIECE_EMPTY))
                            vMoves.push_back(SquareToString(from) + SquareToString(to2));
                    }
                }
            }

            // Captures (including en passant)
            for (int side = -1; side <= 1; side += 2)
            {
                int cf = ff + side;
                int cr = fr + dir;
                if (cf < 0 || cf > 7 || cr < 0 || cr > 7)
                    continue;
                to = MakeSquare(cf, cr);
                bool canCapture = (board[to] != PIECE_EMPTY && PieceColor(board[to]) != color);
                bool isEp = (to == nEnPassantSquare);
                if (canCapture || isEp)
                {
                    if (RankOf(to) == promoRank)
                    {
                        int promos[] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };
                        for (int i = 0; i < 4; i++)
                        {
                            if (IsLegalMoveInternal(from, to, promos[i]))
                            {
                                string s = SquareToString(from) + SquareToString(to);
                                const char pc[] = { 0, 0, 'n', 'b', 'r', 'q' };
                                s += pc[promos[i]];
                                vMoves.push_back(s);
                            }
                        }
                    }
                    else
                    {
                        if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                            vMoves.push_back(SquareToString(from) + SquareToString(to));
                    }
                }
            }
            break;
        }

        case PIECE_KNIGHT:
        {
            static const int knightMoves[8][2] = {
                {-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}
            };
            for (int i = 0; i < 8; i++)
            {
                int tf = ff + knightMoves[i][0];
                int tr = fr + knightMoves[i][1];
                if (tf < 0 || tf > 7 || tr < 0 || tr > 7)
                    continue;
                int to = MakeSquare(tf, tr);
                if (board[to] != PIECE_EMPTY && PieceColor(board[to]) == color)
                    continue;
                if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                    vMoves.push_back(SquareToString(from) + SquareToString(to));
            }
            break;
        }

        case PIECE_BISHOP:
        {
            static const int dirs[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
            for (int d = 0; d < 4; d++)
            {
                int tf = ff + dirs[d][0], tr = fr + dirs[d][1];
                while (tf >= 0 && tf <= 7 && tr >= 0 && tr <= 7)
                {
                    int to = MakeSquare(tf, tr);
                    if (board[to] != PIECE_EMPTY && PieceColor(board[to]) == color)
                        break;
                    if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                        vMoves.push_back(SquareToString(from) + SquareToString(to));
                    if (board[to] != PIECE_EMPTY)
                        break;
                    tf += dirs[d][0];
                    tr += dirs[d][1];
                }
            }
            break;
        }

        case PIECE_ROOK:
        {
            static const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
            for (int d = 0; d < 4; d++)
            {
                int tf = ff + dirs[d][0], tr = fr + dirs[d][1];
                while (tf >= 0 && tf <= 7 && tr >= 0 && tr <= 7)
                {
                    int to = MakeSquare(tf, tr);
                    if (board[to] != PIECE_EMPTY && PieceColor(board[to]) == color)
                        break;
                    if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                        vMoves.push_back(SquareToString(from) + SquareToString(to));
                    if (board[to] != PIECE_EMPTY)
                        break;
                    tf += dirs[d][0];
                    tr += dirs[d][1];
                }
            }
            break;
        }

        case PIECE_QUEEN:
        {
            static const int dirs[8][2] = {
                {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}
            };
            for (int d = 0; d < 8; d++)
            {
                int tf = ff + dirs[d][0], tr = fr + dirs[d][1];
                while (tf >= 0 && tf <= 7 && tr >= 0 && tr <= 7)
                {
                    int to = MakeSquare(tf, tr);
                    if (board[to] != PIECE_EMPTY && PieceColor(board[to]) == color)
                        break;
                    if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                        vMoves.push_back(SquareToString(from) + SquareToString(to));
                    if (board[to] != PIECE_EMPTY)
                        break;
                    tf += dirs[d][0];
                    tr += dirs[d][1];
                }
            }
            break;
        }

        case PIECE_KING:
        {
            // Normal king moves
            for (int dr = -1; dr <= 1; dr++)
            {
                for (int df = -1; df <= 1; df++)
                {
                    if (dr == 0 && df == 0)
                        continue;
                    int tf = ff + df, tr = fr + dr;
                    if (tf < 0 || tf > 7 || tr < 0 || tr > 7)
                        continue;
                    int to = MakeSquare(tf, tr);
                    if (board[to] != PIECE_EMPTY && PieceColor(board[to]) == color)
                        continue;
                    if (IsLegalMoveInternal(from, to, PIECE_EMPTY))
                        vMoves.push_back(SquareToString(from) + SquareToString(to));
                }
            }

            // Castling moves
            if (fWhiteToMove && from == 4)
            {
                if (IsLegalMoveInternal(4, 6, PIECE_EMPTY))
                    vMoves.push_back("e1g1");
                if (IsLegalMoveInternal(4, 2, PIECE_EMPTY))
                    vMoves.push_back("e1c1");
            }
            else if (!fWhiteToMove && from == 60)
            {
                if (IsLegalMoveInternal(60, 62, PIECE_EMPTY))
                    vMoves.push_back("e8g8");
                if (IsLegalMoveInternal(60, 58, PIECE_EMPTY))
                    vMoves.push_back("e8c8");
            }
            break;
        }
        } // switch
    }

    return vMoves;
}



//
// IsCheckmate - in check and no legal moves
//
bool CChessBoard::IsCheckmate()
{
    if (!IsCheck())
        return false;
    return GetLegalMoves().empty();
}



//
// IsStalemate - not in check and no legal moves
//
bool CChessBoard::IsStalemate()
{
    if (IsCheck())
        return false;
    return GetLegalMoves().empty();
}



//
// IsDraw - stalemate or 50-move rule
//
bool CChessBoard::IsDraw()
{
    if (IsStalemate())
        return true;
    if (nHalfMoveClock >= 100) // 50 moves = 100 half-moves
        return true;
    return false;
}



//
// ToASCII - render the board as an 8-line ASCII string for TUI display
//
//   8 | r n b q k b n r
//   7 | p p p p p p p p
//   6 | . . . . . . . .
//   ...
//   1 | R N B Q K B N R
//     +----------------
//       a b c d e f g h
//
string CChessBoard::ToASCII() const
{
    static const char pieceChars[] = ".pnbrqk";
    string s;

    for (int rank = 7; rank >= 0; rank--)
    {
        s += (char)('1' + rank);
        s += " | ";
        for (int file = 0; file < 8; file++)
        {
            unsigned char p = board[MakeSquare(file, rank)];
            char c;
            if (p == PIECE_EMPTY)
            {
                c = '.';
            }
            else
            {
                c = pieceChars[PieceType(p)];
                if (IsWhite(p))
                    c = (char)toupper(c);
            }
            s += c;
            if (file < 7)
                s += ' ';
        }
        s += '\n';
    }
    s += "  +----------------\n";
    s += "    a b c d e f g h\n";

    return s;
}
