// Copyright (c) 2026 Bcash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// ncurses TUI - the real deal

#include "headers.h"
#include "sha.h"
#include "rpc.h"
#include "bgold.h"
#include <ncurses.h>

// Stubs for GUI functions and data referenced by other code
void MainFrameRepaint() {}
map<string, string> mapAddressBook;

// Solo mining mode
bool fSoloMine = false;

// Shutdown handling
extern bool fShutdown;
static volatile bool fRequestShutdown = false;

void HandleSignal(int sig)
{
    fRequestShutdown = true;
    fShutdown = true;
}

// ── Color pairs ──────────────────────────────────────────────
enum Colors {
    C_TITLE = 1,
    C_STATUS,
    C_TAB_ACTIVE,
    C_TAB_INACTIVE,
    C_BORDER,
    C_HEADER,
    C_TX_POS,
    C_TX_NEG,
    C_TX_ZERO,
    C_HELP,
    C_ACCENT,
    C_MINING,
    C_BGOLD,
    C_DIM,
    C_SEND_FIELD,
    C_SEND_OK,
    C_SEND_ERR,
};

static void InitColors()
{
    if (!has_colors()) return;
    start_color();
    use_default_colors();

    // pair id         fg              bg
    init_pair(C_TITLE,       COLOR_CYAN,    -1);
    init_pair(C_STATUS,      COLOR_WHITE,   COLOR_BLUE);
    init_pair(C_TAB_ACTIVE,  COLOR_BLACK,   COLOR_CYAN);
    init_pair(C_TAB_INACTIVE,COLOR_CYAN,    -1);
    init_pair(C_BORDER,      COLOR_CYAN,    -1);
    init_pair(C_HEADER,      COLOR_YELLOW,  -1);
    init_pair(C_TX_POS,      COLOR_GREEN,   -1);
    init_pair(C_TX_NEG,      COLOR_RED,     -1);
    init_pair(C_TX_ZERO,     COLOR_WHITE,   -1);
    init_pair(C_HELP,        COLOR_BLACK,   COLOR_CYAN);
    init_pair(C_ACCENT,      COLOR_MAGENTA, -1);
    init_pair(C_MINING,      COLOR_YELLOW,  -1);
    init_pair(C_BGOLD,       COLOR_YELLOW,  -1);
    init_pair(C_DIM,         COLOR_CYAN,    -1);
    init_pair(C_SEND_FIELD,  COLOR_WHITE,   COLOR_BLUE);
    init_pair(C_SEND_OK,     COLOR_GREEN,   -1);
    init_pair(C_SEND_ERR,    COLOR_RED,     -1);
}


// ── TUI state ────────────────────────────────────────────────
enum Tab { TAB_WALLET=0, TAB_NEWS, TAB_MARKET, TAB_BGOLD, TAB_SEND, TAB_COUNT };
static int nCurrentTab = TAB_WALLET;
static int nScrollOffset = 0;
static int nContentLines = 0;
static int nAnimFrame = 0;

// Mining activity tracking
static int nLastKnownHeight = 0;
static int64 nLastBlockTime = 0;
static int nBlocksThisSession = 0;

// Send form state
static string strSendAddress;
static string strSendAmount;
static int nSendField = 0;
static string strSendStatus;
static bool fSendError = false;

// Windows
static WINDOW *winHeader = NULL;
static WINDOW *winStatus = NULL;
static WINDOW *winTabs   = NULL;
static WINDOW *winContent = NULL;
static WINDOW *winHelp   = NULL;

// ── ASCII banner ─────────────────────────────────────────────
static const char* banner[] = {
    " ____   ____          _     ",
    "|  _ \\ / ___|__ _ ___| |__  ",
    "| |_) | |   / _` / __| '_ \\ ",
    "|  _ <| |__| (_| \\__ \\ | | |",
    "|_| \\_\\\\____\\__,_|___/_| |_|",
    NULL
};

static const int BANNER_HEIGHT = 5;
static const int BANNER_WIDTH  = 29;


// ── Window management ────────────────────────────────────────
static void CreateWindows()
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int headerH = BANNER_HEIGHT + 2;
    int statusH = 3;
    int tabsH   = 3;
    int helpH   = 3;
    int contentH = rows - headerH - statusH - tabsH - helpH;
    if (contentH < 3) contentH = 3;

    int y = 0;
    winHeader  = newwin(headerH, cols, y, 0);          y += headerH;
    winStatus  = newwin(statusH, cols, y, 0);           y += statusH;
    winTabs    = newwin(tabsH, cols, y, 0);             y += tabsH;
    winContent = newwin(contentH, cols, y, 0);          y += contentH;
    winHelp    = newwin(helpH, cols, y, 0);
}

static void DestroyWindows()
{
    if (winHeader)  { delwin(winHeader);  winHeader  = NULL; }
    if (winStatus)  { delwin(winStatus);  winStatus  = NULL; }
    if (winTabs)    { delwin(winTabs);    winTabs    = NULL; }
    if (winContent) { delwin(winContent); winContent = NULL; }
    if (winHelp)    { delwin(winHelp);    winHelp    = NULL; }
}


// ── Utility: draw a box with color ──────────────────────────
static void ColorBox(WINDOW* win, int colorPair)
{
    wattron(win, COLOR_PAIR(colorPair));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(colorPair));
}


// ── Mining spinner ───────────────────────────────────────────
static const char* spinChars = "|/-\\";

static string MiningSpinner()
{
    char buf[4];
    buf[0] = spinChars[nAnimFrame % 4];
    buf[1] = 0;
    return string(buf);
}


// ── Sparkline from recent block times ────────────────────────
static string BlockSparkline()
{
    // Show block height history as a mini bar chart
    const char* sparks = " _.:oO@#";
    int nLevels = 8;
    string result;

    // Use last 16 block times relative to each other
    vector<unsigned int> vTimes;
    CRITICAL_BLOCK(cs_main)
    {
        CBlockIndex* pindex = pindexBest;
        for (int i = 0; i < 16 && pindex && pindex->pprev; i++)
        {
            unsigned int dt = pindex->nTime - pindex->pprev->nTime;
            vTimes.push_back(dt);
            pindex = pindex->pprev;
        }
    }
    if (vTimes.empty()) return "[no blocks]";

    // Reverse so newest is on right
    reverse(vTimes.begin(), vTimes.end());

    // Find range
    unsigned int nMax = *max_element(vTimes.begin(), vTimes.end());
    if (nMax == 0) nMax = 1;

    for (int i = 0; i < (int)vTimes.size(); i++)
    {
        // Invert: fast blocks get tall bars, slow blocks get short
        int level = nLevels - 1 - (int)((int64)vTimes[i] * (nLevels - 1) / nMax);
        if (level < 0) level = 0;
        if (level >= nLevels) level = nLevels - 1;
        result += sparks[level];
    }
    return result;
}


// ── Header with ASCII art ────────────────────────────────────
static void DrawHeader()
{
    int rows, cols;
    getmaxyx(winHeader, rows, cols);

    werase(winHeader);
    ColorBox(winHeader, C_BORDER);

    // Draw banner centered
    int startX = (cols - BANNER_WIDTH) / 2;
    if (startX < 2) startX = 2;

    wattron(winHeader, COLOR_PAIR(C_TITLE) | A_BOLD);
    for (int i = 0; banner[i]; i++)
        mvwprintw(winHeader, 1 + i, startX, "%s", banner[i]);
    wattroff(winHeader, COLOR_PAIR(C_TITLE) | A_BOLD);

    // Version + tagline on the right
    wattron(winHeader, COLOR_PAIR(C_DIM));
    mvwprintw(winHeader, 2, cols - 20, "v0.1.0");
    mvwprintw(winHeader, 3, cols - 20, "p2p digital cash");
    wattroff(winHeader, COLOR_PAIR(C_DIM));

    // Solo mode indicator on top right
    if (fSoloMine)
    {
        wattron(winHeader, COLOR_PAIR(C_MINING) | A_BOLD);
        mvwprintw(winHeader, 1, cols - 14, "[SOLO MODE]");
        wattroff(winHeader, COLOR_PAIR(C_MINING) | A_BOLD);
    }

    wnoutrefresh(winHeader);
}


// ── Status bar ───────────────────────────────────────────────
static void DrawStatusBar()
{
    int rows, cols;
    getmaxyx(winStatus, rows, cols);
    (void)rows;

    werase(winStatus);

    // Full-width colored background
    wattron(winStatus, COLOR_PAIR(C_STATUS));
    for (int y = 0; y < 3; y++)
        mvwhline(winStatus, y, 0, ' ', cols);

    int nPeers = 0;
    CRITICAL_BLOCK(cs_vNodes)
    {
        nPeers = (int)vNodes.size();
    }

    int64 nBalance = GetBalance();
    int64 nBgoldBal = 0;
    int bgoldHeight = 0;
    CRITICAL_BLOCK(cs_bgold)
    {
        nBgoldBal = GetBgoldBalance(keyUser.GetPubKey());
        bgoldHeight = nBgoldHeight;
    }

    // Track new blocks
    if (nBestHeight > nLastKnownHeight)
    {
        nBlocksThisSession += (nBestHeight - nLastKnownHeight);
        nLastBlockTime = GetTime();
        nLastKnownHeight = nBestHeight;
    }

    // Mining indicator
    string strMine;
    if (fGenerateBitcoins)
    {
        wattron(winStatus, A_BOLD);
        mvwprintw(winStatus, 1, 2, "%s MINING", MiningSpinner().c_str());
        wattroff(winStatus, A_BOLD);
    }

    // Block info
    string sparkline = BlockSparkline();
    mvwprintw(winStatus, 0, 2, " BLK %d ", nBestHeight);
    mvwprintw(winStatus, 0, 14, "PEERS %d ", nPeers);
    if (nBlocksThisSession > 0)
        mvwprintw(winStatus, 0, 25, "MINED %d ", nBlocksThisSession);

    // Sparkline
    mvwprintw(winStatus, 0, cols - (int)sparkline.size() - 3, "[%s]", sparkline.c_str());

    // Balance line
    mvwprintw(winStatus, 1, cols / 3, "BCASH: %s", FormatMoney(nBalance).c_str());

    // Bgold
    wattron(winStatus, A_BOLD);
    mvwprintw(winStatus, 1, cols * 2 / 3, "BGOLD: %s", FormatMoney(nBgoldBal).c_str());
    wattroff(winStatus, A_BOLD);

    // Time since last block
    if (nLastBlockTime > 0)
    {
        int64 nAge = GetTime() - nLastBlockTime;
        if (nAge < 60)
            mvwprintw(winStatus, 2, 2, "Last block: %ds ago", (int)nAge);
        else if (nAge < 3600)
            mvwprintw(winStatus, 2, 2, "Last block: %dm ago", (int)(nAge / 60));
        else
            mvwprintw(winStatus, 2, 2, "Last block: %dh ago", (int)(nAge / 3600));
    }

    // Uptime
    static int64 nStartTime = GetTime();
    int64 nUptime = GetTime() - nStartTime;
    int uh = nUptime / 3600;
    int um = (nUptime % 3600) / 60;
    int us = nUptime % 60;
    mvwprintw(winStatus, 2, cols - 18, "UP %02d:%02d:%02d", uh, um, us);

    wattroff(winStatus, COLOR_PAIR(C_STATUS));
    wnoutrefresh(winStatus);
}


// ── Tab bar ──────────────────────────────────────────────────
static void DrawTabBar()
{
    int rows, cols;
    getmaxyx(winTabs, rows, cols);
    (void)rows;

    werase(winTabs);
    ColorBox(winTabs, C_BORDER);

    const char* tabNames[] = { "WALLET", "NEWS", "MARKET", "BGOLD", "SEND" };
    const char* tabIcons[] = { "$", "#", "%", "G", ">" };
    int x = 2;
    for (int i = 0; i < TAB_COUNT; i++)
    {
        if (i == nCurrentTab)
        {
            wattron(winTabs, COLOR_PAIR(C_TAB_ACTIVE) | A_BOLD);
            mvwprintw(winTabs, 1, x, " %s %d:%s ", tabIcons[i], i + 1, tabNames[i]);
            wattroff(winTabs, COLOR_PAIR(C_TAB_ACTIVE) | A_BOLD);
            x += strlen(tabNames[i]) + 7;
        }
        else
        {
            wattron(winTabs, COLOR_PAIR(C_TAB_INACTIVE));
            mvwprintw(winTabs, 1, x, " %d:%s ", i + 1, tabNames[i]);
            wattroff(winTabs, COLOR_PAIR(C_TAB_INACTIVE));
            x += strlen(tabNames[i]) + 5;
        }
    }

    wnoutrefresh(winTabs);
}


// ── Wallet tab ───────────────────────────────────────────────
static void DrawWalletTab()
{
    int rows, cols;
    getmaxyx(winContent, rows, cols);

    werase(winContent);
    ColorBox(winContent, C_BORDER);

    wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    mvwprintw(winContent, 0, 2, " $ Wallet Transactions ");
    wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);

    int line = 1;

    // Column header
    wattron(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);
    mvwprintw(winContent, line, 2, "  %-12s %15s %6s  %s", "TXID", "AMOUNT", "CONF", "TIME");
    wattroff(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);

    // Separator line
    line++;
    wattron(winContent, COLOR_PAIR(C_DIM));
    mvwhline(winContent, line, 1, ACS_HLINE, cols - 2);
    wattroff(winContent, COLOR_PAIR(C_DIM));
    line++;

    // Collect wallet transactions
    vector<pair<int64, const CWalletTx*> > vSorted;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin();
             it != mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = it->second;
            vSorted.push_back(make_pair(wtx.GetTxTime(), &wtx));
        }
    }
    sort(vSorted.begin(), vSorted.end(),
         [](const pair<int64, const CWalletTx*>& a, const pair<int64, const CWalletTx*>& b) {
             return a.first > b.first;
         });

    nContentLines = (int)vSorted.size();

    for (int i = nScrollOffset; i < (int)vSorted.size() && line < rows - 1; i++, line++)
    {
        const CWalletTx* pwtx = vSorted[i].second;

        int64 nCredit = pwtx->GetCredit();
        int64 nDebit = pwtx->GetDebit();
        int64 nNet = nCredit - nDebit;
        int nConf = pwtx->GetDepthInMainChain();
        string hashStr = pwtx->GetHash().ToString().substr(0, 12);
        string timeStr = DateTimeStr(pwtx->GetTxTime());
        string amountStr = FormatMoney(nNet);

        // Color based on direction
        int color;
        string indicator;
        if (nNet > 0)      { color = C_TX_POS; indicator = ">>"; }
        else if (nNet < 0) { color = C_TX_NEG; indicator = "<<"; }
        else               { color = C_TX_ZERO; indicator = "--"; }

        // Confirmation indicator
        string confStr;
        if (nConf == 0)       confStr = " mem ";
        else if (nConf < 6)   confStr = strprintf(" %d/6 ", nConf);
        else                  confStr = strprintf(" %4d ", nConf);

        wattron(winContent, COLOR_PAIR(color));
        mvwprintw(winContent, line, 2, "%s", indicator.c_str());
        wattroff(winContent, COLOR_PAIR(color));

        mvwprintw(winContent, line, 5, "%-12s", hashStr.c_str());

        wattron(winContent, COLOR_PAIR(color) | A_BOLD);
        mvwprintw(winContent, line, 18, "%15s", amountStr.c_str());
        wattroff(winContent, COLOR_PAIR(color) | A_BOLD);

        if (nConf < 6)
            wattron(winContent, COLOR_PAIR(C_MINING));
        mvwprintw(winContent, line, 34, "%s", confStr.c_str());
        if (nConf < 6)
            wattroff(winContent, COLOR_PAIR(C_MINING));

        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, line, 41, "%s", timeStr.c_str());
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    if (vSorted.empty())
    {
        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, rows / 2, (cols - 28) / 2, "~ no transactions yet ~");
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    wnoutrefresh(winContent);
}


// ── News tab ─────────────────────────────────────────────────
static void DrawNewsTab()
{
    int rows, cols;
    getmaxyx(winContent, rows, cols);

    werase(winContent);
    ColorBox(winContent, C_BORDER);

    wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    mvwprintw(winContent, 0, 2, " # News Feed ");
    wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);

    int line = 1;

    wattron(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);
    mvwprintw(winContent, line, 2, " ##  PTS  VOTES  %-*s  AGE", cols - 28, "TITLE");
    wattroff(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);

    line++;
    wattron(winContent, COLOR_PAIR(C_DIM));
    mvwhline(winContent, line, 1, ACS_HLINE, cols - 2);
    wattroff(winContent, COLOR_PAIR(C_DIM));
    line++;

    vector<CNewsItem> vNews;
    CRITICAL_BLOCK(cs_mapNews)
    {
        vNews = GetTopNews(20);
    }

    nContentLines = (int)vNews.size();

    for (int i = nScrollOffset; i < (int)vNews.size() && line < rows - 1; i++, line++)
    {
        const CNewsItem& item = vNews[i];
        double score = GetNewsScore(item.nVotes, item.nTime);

        int64 nAge = GetTime() - item.nTime;
        string ageStr;
        if (nAge < 3600)
            ageStr = strprintf("%dm", (int)(nAge / 60));
        else if (nAge < 86400)
            ageStr = strprintf("%dh", (int)(nAge / 3600));
        else
            ageStr = strprintf("%dd", (int)(nAge / 86400));

        int titleWidth = cols - 28;
        if (titleWidth < 10) titleWidth = 10;
        string title = item.strTitle;
        if ((int)title.size() > titleWidth)
            title = title.substr(0, titleWidth - 3) + "...";

        // Rank number
        wattron(winContent, COLOR_PAIR(C_ACCENT) | A_BOLD);
        mvwprintw(winContent, line, 2, "%3d.", i + 1);
        wattroff(winContent, COLOR_PAIR(C_ACCENT) | A_BOLD);

        // Score
        wattron(winContent, COLOR_PAIR(C_TX_POS));
        mvwprintw(winContent, line, 7, "%4.0f", score);
        wattroff(winContent, COLOR_PAIR(C_TX_POS));

        mvwprintw(winContent, line, 12, "%5d", item.nVotes);

        // Title
        wattron(winContent, A_BOLD);
        mvwprintw(winContent, line, 19, "%-*s", titleWidth, title.c_str());
        wattroff(winContent, A_BOLD);

        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, line, 19 + titleWidth + 1, "%4s", ageStr.c_str());
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    if (vNews.empty())
    {
        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, rows / 2, (cols - 20) / 2, "~ no news items ~");
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    wnoutrefresh(winContent);
}


// ── Market tab ───────────────────────────────────────────────
static void DrawMarketTab()
{
    int rows, cols;
    getmaxyx(winContent, rows, cols);

    werase(winContent);
    ColorBox(winContent, C_BORDER);

    wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    mvwprintw(winContent, 0, 2, " %% Marketplace ");
    wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);

    int line = 1;

    int titleW = cols - 44;
    if (titleW < 10) titleW = 10;

    wattron(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);
    mvwprintw(winContent, line, 2, "%-*s %-12s %14s  %-10s", titleW, "PRODUCT", "CATEGORY", "PRICE", "SELLER");
    wattroff(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);

    line++;
    wattron(winContent, COLOR_PAIR(C_DIM));
    mvwhline(winContent, line, 1, ACS_HLINE, cols - 2);
    wattroff(winContent, COLOR_PAIR(C_DIM));
    line++;

    vector<pair<uint256, CProduct> > vProducts;
    CRITICAL_BLOCK(cs_mapProducts)
    {
        for (map<uint256, CProduct>::iterator it = mapProducts.begin();
             it != mapProducts.end(); ++it)
        {
            vProducts.push_back(*it);
        }
    }

    nContentLines = (int)vProducts.size();

    for (int i = nScrollOffset; i < (int)vProducts.size() && line < rows - 1; i++, line++)
    {
        const CProduct& prod = vProducts[i].second;

        string title = "(untitled)";
        string category = "";
        string price = "";
        string seller = "";

        map<string, string>::const_iterator mi;
        mi = prod.mapValue.find("title");
        if (mi != prod.mapValue.end()) title = mi->second;
        mi = prod.mapValue.find("category");
        if (mi != prod.mapValue.end()) category = mi->second;
        mi = prod.mapValue.find("price");
        if (mi != prod.mapValue.end()) price = mi->second;
        mi = prod.mapValue.find("seller");
        if (mi != prod.mapValue.end()) seller = mi->second;

        if ((int)title.size() > titleW)
            title = title.substr(0, titleW - 3) + "...";

        wattron(winContent, A_BOLD);
        mvwprintw(winContent, line, 2, "%-*s", titleW, title.c_str());
        wattroff(winContent, A_BOLD);

        wattron(winContent, COLOR_PAIR(C_ACCENT));
        mvwprintw(winContent, line, 2 + titleW + 1, "%-12s", category.substr(0, 12).c_str());
        wattroff(winContent, COLOR_PAIR(C_ACCENT));

        wattron(winContent, COLOR_PAIR(C_TX_POS) | A_BOLD);
        mvwprintw(winContent, line, 2 + titleW + 14, "%14s", price.substr(0, 14).c_str());
        wattroff(winContent, COLOR_PAIR(C_TX_POS) | A_BOLD);

        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, line, 2 + titleW + 30, "%-10s", seller.substr(0, 10).c_str());
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    if (vProducts.empty())
    {
        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, rows / 2, (cols - 24) / 2, "~ no products listed ~");
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    wnoutrefresh(winContent);
}


// ── Bgold tab ────────────────────────────────────────────────
static void DrawBgoldTab()
{
    int rows, cols;
    getmaxyx(winContent, rows, cols);

    werase(winContent);
    ColorBox(winContent, C_BORDER);

    wattron(winContent, COLOR_PAIR(C_BGOLD) | A_BOLD);
    mvwprintw(winContent, 0, 2, " G Bgold Sidechain ");
    wattroff(winContent, COLOR_PAIR(C_BGOLD) | A_BOLD);

    int line = 2;

    int bgoldH = 0;
    int64 bgoldBal = 0;
    uint256 bestHash;
    vector<pair<uint256, CBgoldBlock> > vBlocks;

    CRITICAL_BLOCK(cs_bgold)
    {
        bgoldH = nBgoldHeight;
        bgoldBal = GetBgoldBalance(keyUser.GetPubKey());
        bestHash = hashBestBgoldBlock;

        for (map<uint256, CBgoldBlock>::iterator it = mapBgoldBlocks.begin();
             it != mapBgoldBlocks.end(); ++it)
        {
            vBlocks.push_back(*it);
        }
    }

    sort(vBlocks.begin(), vBlocks.end(),
         [](const pair<uint256, CBgoldBlock>& a, const pair<uint256, CBgoldBlock>& b) {
             return a.second.nHeight > b.second.nHeight;
         });
    if (vBlocks.size() > 20) vBlocks.resize(20);

    // Summary cards
    wattron(winContent, COLOR_PAIR(C_BGOLD) | A_BOLD);
    mvwprintw(winContent, line, 4, "CHAIN HEIGHT");
    mvwprintw(winContent, line, 22, "BALANCE");
    mvwprintw(winContent, line, 42, "BEST BLOCK");
    wattroff(winContent, COLOR_PAIR(C_BGOLD) | A_BOLD);
    line++;

    wattron(winContent, A_BOLD);
    mvwprintw(winContent, line, 4, "%d", bgoldH);
    wattroff(winContent, A_BOLD);

    wattron(winContent, COLOR_PAIR(C_TX_POS) | A_BOLD);
    mvwprintw(winContent, line, 22, "%s BGOLD", FormatMoney(bgoldBal).c_str());
    wattroff(winContent, COLOR_PAIR(C_TX_POS) | A_BOLD);

    wattron(winContent, COLOR_PAIR(C_DIM));
    mvwprintw(winContent, line, 42, "%s", bestHash.ToString().substr(0, 24).c_str());
    wattroff(winContent, COLOR_PAIR(C_DIM));

    line += 2;
    wattron(winContent, COLOR_PAIR(C_DIM));
    mvwhline(winContent, line, 1, ACS_HLINE, cols - 2);
    wattroff(winContent, COLOR_PAIR(C_DIM));
    line++;

    // Block table header
    wattron(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);
    mvwprintw(winContent, line, 2, " HEIGHT  %-26s %-26s", "BGOLD HASH", "BCASH ANCHOR");
    wattroff(winContent, COLOR_PAIR(C_HEADER) | A_BOLD);
    line++;

    nContentLines = (int)vBlocks.size() + 8;

    for (int i = nScrollOffset; i < (int)vBlocks.size() && line < rows - 1; i++, line++)
    {
        const CBgoldBlock& blk = vBlocks[i].second;

        wattron(winContent, COLOR_PAIR(C_BGOLD) | A_BOLD);
        mvwprintw(winContent, line, 2, " %5d", blk.nHeight);
        wattroff(winContent, COLOR_PAIR(C_BGOLD) | A_BOLD);

        mvwprintw(winContent, line, 10, "%-26s", vBlocks[i].first.ToString().substr(0, 24).c_str());

        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, line, 37, "%-26s", blk.hashBcashBlock.ToString().substr(0, 24).c_str());
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    if (vBlocks.empty())
    {
        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, rows / 2 + 1, (cols - 24) / 2, "~ no bgold blocks yet ~");
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }

    wnoutrefresh(winContent);
}


// ── Send tab ─────────────────────────────────────────────────
static void DrawSendTab()
{
    int rows, cols;
    getmaxyx(winContent, rows, cols);

    werase(winContent);
    ColorBox(winContent, C_BORDER);

    wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    mvwprintw(winContent, 0, 2, " > Send BCASH ");
    wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);

    int line = 2;
    int fieldX = 14;
    int fieldW = cols - fieldX - 4;
    if (fieldW > 40) fieldW = 40;

    // Balance display
    wattron(winContent, COLOR_PAIR(C_TX_POS) | A_BOLD);
    mvwprintw(winContent, line, cols - 30, "Balance: %s BCASH", FormatMoney(GetBalance()).c_str());
    wattroff(winContent, COLOR_PAIR(C_TX_POS) | A_BOLD);
    line += 2;

    // Address label
    wattron(winContent, COLOR_PAIR(C_HEADER));
    mvwprintw(winContent, line, 4, "Address:");
    wattroff(winContent, COLOR_PAIR(C_HEADER));

    // Address field
    if (nSendField == 0)
        wattron(winContent, COLOR_PAIR(C_SEND_FIELD));
    else
        wattron(winContent, COLOR_PAIR(C_DIM));
    mvwprintw(winContent, line, fieldX, "[%-*s]", fieldW, strSendAddress.c_str());
    if (nSendField == 0)
    {
        wattroff(winContent, COLOR_PAIR(C_SEND_FIELD));
        // Cursor indicator
        wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
        mvwprintw(winContent, line, 2, ">>");
        wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    }
    else
        wattroff(winContent, COLOR_PAIR(C_DIM));
    line += 2;

    // Amount label
    wattron(winContent, COLOR_PAIR(C_HEADER));
    mvwprintw(winContent, line, 4, "Amount:");
    wattroff(winContent, COLOR_PAIR(C_HEADER));

    // Amount field
    if (nSendField == 1)
        wattron(winContent, COLOR_PAIR(C_SEND_FIELD));
    else
        wattron(winContent, COLOR_PAIR(C_DIM));
    mvwprintw(winContent, line, fieldX, "[%-14s] BCASH", strSendAmount.c_str());
    if (nSendField == 1)
    {
        wattroff(winContent, COLOR_PAIR(C_SEND_FIELD));
        wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
        mvwprintw(winContent, line, 2, ">>");
        wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    }
    else
        wattroff(winContent, COLOR_PAIR(C_DIM));
    line += 2;

    // Send button
    if (nSendField == 2)
    {
        wattron(winContent, COLOR_PAIR(C_TAB_ACTIVE) | A_BOLD);
        mvwprintw(winContent, line, fieldX, " [ CONFIRM SEND ] ");
        wattroff(winContent, COLOR_PAIR(C_TAB_ACTIVE) | A_BOLD);
        wattron(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
        mvwprintw(winContent, line, 2, ">>");
        wattroff(winContent, COLOR_PAIR(C_TITLE) | A_BOLD);
    }
    else
    {
        wattron(winContent, COLOR_PAIR(C_DIM));
        mvwprintw(winContent, line, fieldX, " [ CONFIRM SEND ] ");
        wattroff(winContent, COLOR_PAIR(C_DIM));
    }
    line += 2;

    // Separator
    wattron(winContent, COLOR_PAIR(C_DIM));
    mvwhline(winContent, line, 1, ACS_HLINE, cols - 2);
    wattroff(winContent, COLOR_PAIR(C_DIM));
    line++;

    // Status message
    if (!strSendStatus.empty())
    {
        if (fSendError)
            wattron(winContent, COLOR_PAIR(C_SEND_ERR) | A_BOLD);
        else
            wattron(winContent, COLOR_PAIR(C_SEND_OK) | A_BOLD);
        mvwprintw(winContent, line, 4, "%s", strSendStatus.c_str());
        if (fSendError)
            wattroff(winContent, COLOR_PAIR(C_SEND_ERR) | A_BOLD);
        else
            wattroff(winContent, COLOR_PAIR(C_SEND_OK) | A_BOLD);
    }

    nContentLines = 0;
    wnoutrefresh(winContent);
}


// ── Help bar ─────────────────────────────────────────────────
static void DrawHelpBar()
{
    int rows, cols;
    getmaxyx(winHelp, rows, cols);
    (void)rows;

    werase(winHelp);
    wattron(winHelp, COLOR_PAIR(C_HELP));
    for (int y = 0; y < 3; y++)
        mvwhline(winHelp, y, 0, ' ', cols);

    if (nCurrentTab == TAB_SEND)
        mvwprintw(winHelp, 1, 2, " q:Quit  1-5:Tabs  Tab/Arrow:Fields  Enter:Send  Esc:Clear ");
    else
        mvwprintw(winHelp, 1, 2, " q:Quit  1-5:Tabs  j/k/Arrows:Scroll  PgUp/PgDn  r:Refresh ");

    wattroff(winHelp, COLOR_PAIR(C_HELP));
    wnoutrefresh(winHelp);
}


// ── Draw everything ──────────────────────────────────────────
static void DrawContent()
{
    switch (nCurrentTab)
    {
    case TAB_WALLET: DrawWalletTab(); break;
    case TAB_NEWS:   DrawNewsTab();   break;
    case TAB_MARKET: DrawMarketTab(); break;
    case TAB_BGOLD:  DrawBgoldTab();  break;
    case TAB_SEND:   DrawSendTab();   break;
    }
}

static void DrawAll()
{
    DrawHeader();
    DrawStatusBar();
    DrawTabBar();
    DrawContent();
    DrawHelpBar();
    doupdate();
}

static void SwitchTab(int tab)
{
    if (tab >= 0 && tab < TAB_COUNT && tab != nCurrentTab)
    {
        nCurrentTab = tab;
        nScrollOffset = 0;
    }
}


// ── Send form logic ──────────────────────────────────────────
static void DoSend()
{
    strSendStatus = "";
    fSendError = false;

    if (strSendAddress.empty())
    {
        strSendStatus = "ERROR: address is empty";
        fSendError = true;
        return;
    }
    if (strSendAmount.empty())
    {
        strSendStatus = "ERROR: amount is empty";
        fSendError = true;
        return;
    }

    uint160 hash160;
    if (!AddressToHash160(strSendAddress, hash160))
    {
        strSendStatus = "ERROR: invalid bcash address";
        fSendError = true;
        return;
    }

    double dAmount = atof(strSendAmount.c_str());
    if (dAmount <= 0.0)
    {
        strSendStatus = "ERROR: invalid amount";
        fSendError = true;
        return;
    }
    int64 nAmount = (int64)(dAmount * COIN + 0.5);

    if (nAmount > GetBalance())
    {
        strSendStatus = "ERROR: insufficient balance";
        fSendError = true;
        return;
    }

    CScript scriptPubKey;
    scriptPubKey << OP_DUP << OP_HASH160 << hash160 << OP_EQUALVERIFY << OP_CHECKSIG;

    CWalletTx wtx;
    if (!SendMoney(scriptPubKey, nAmount, wtx))
    {
        strSendStatus = "ERROR: transaction failed";
        fSendError = true;
        return;
    }

    strSendStatus = strprintf("SENT %s BCASH -> %s  tx:%s",
        FormatMoney(nAmount).c_str(),
        strSendAddress.substr(0, 12).c_str(),
        wtx.GetHash().ToString().substr(0, 12).c_str());
    fSendError = false;

    strSendAddress.clear();
    strSendAmount.clear();
    nSendField = 0;
}

static void HandleSendInput(int ch)
{
    if (ch == '\t' || ch == KEY_DOWN)
    {
        nSendField = (nSendField + 1) % 3;
        return;
    }
    if (ch == KEY_UP)
    {
        nSendField = (nSendField + 2) % 3;
        return;
    }

    if (ch == 27)
    {
        strSendAddress.clear();
        strSendAmount.clear();
        strSendStatus.clear();
        nSendField = 0;
        return;
    }

    if (ch == '\n' || ch == KEY_ENTER)
    {
        if (nSendField == 2)
            DoSend();
        else
            nSendField = (nSendField + 1) % 3;
        return;
    }

    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
    {
        if (nSendField == 0 && !strSendAddress.empty())
            strSendAddress.erase(strSendAddress.size() - 1);
        else if (nSendField == 1 && !strSendAmount.empty())
            strSendAmount.erase(strSendAmount.size() - 1);
        return;
    }

    if (ch >= 32 && ch <= 126)
    {
        if (nSendField == 0 && strSendAddress.size() < 34)
            strSendAddress += (char)ch;
        else if (nSendField == 1 && strSendAmount.size() < 14)
        {
            if (isdigit(ch) || ch == '.')
                strSendAmount += (char)ch;
        }
    }
}


// ── Main ─────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    bool fGenerate = true;

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "-nogenerate" || arg == "-nogen")
            fGenerate = false;
        else if (arg == "-datadir" && i + 1 < argc)
            strSetDataDir = argv[++i];
        else if (arg == "-debug")
            fDebug = true;
        else if (arg == "-solo")
            fSoloMine = true;
        else if (arg == "-help" || arg == "-h")
        {
            printf("Usage: bcash [options]\n");
            printf("Options:\n");
            printf("  -nogenerate     Don't mine blocks\n");
            printf("  -solo           Mine without peers (solo/bootstrap mode)\n");
            printf("  -datadir <dir>  Data directory\n");
            printf("  -debug          Enable debug output\n");
            printf("  -help           This help message\n");
            return 0;
        }
    }

    fGenerateBitcoins = fGenerate;

    printf("Bcash v0.1.0 - loading...\n");

    if (!LoadAddresses())
        fprintf(stderr, "Warning: Could not load addresses\n");

    if (!LoadBlockIndex())
    {
        fprintf(stderr, "Error loading block index\n");
        return 1;
    }

    if (!LoadWallet())
    {
        fprintf(stderr, "Error loading wallet\n");
        return 1;
    }

    string strError;
    if (!StartNode(strError))
    {
        fprintf(stderr, "Error: %s\n", strError.c_str());
        return 1;
    }

    // Start RPC server
    {
        pthread_t thrRPC;
        if (pthread_create(&thrRPC, NULL,
            [](void* p) -> void* { ThreadRPCServer(p); return NULL; }, NULL) != 0)
            fprintf(stderr, "Warning: Failed to start RPC server\n");
        else
            pthread_detach(thrRPC);
    }

    // Start miner thread
    if (fGenerateBitcoins)
    {
        pthread_t thrMiner;
        if (pthread_create(&thrMiner, NULL,
            [](void* p) -> void* { ThreadBitcoinMiner(p); return NULL; }, NULL) != 0)
            fprintf(stderr, "Warning: Failed to start miner\n");
        else
            pthread_detach(thrMiner);
    }

    // Signal handlers
#ifndef _WIN32
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);
    signal(SIGPIPE, SIG_IGN);
#endif

    // Snapshot initial height
    nLastKnownHeight = nBestHeight;

    // Redirect stdout/stderr to log file so background thread
    // printf() calls don't corrupt the ncurses display
    {
        string strLogFile = GetAppDir() + "/debug.log";
        FILE* flog = fopen(strLogFile.c_str(), "a");
        if (flog)
        {
            fflush(stdout);
            fflush(stderr);
            dup2(fileno(flog), STDOUT_FILENO);
            dup2(fileno(flog), STDERR_FILENO);
            fclose(flog);
        }
    }

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    halfdelay(10);

    InitColors();
    CreateWindows();
    DrawAll();

    // Main event loop
    int64 nLastFullRedraw = 0;

    while (!fRequestShutdown && !fShutdown)
    {
        int ch = getch();

        if (ch == ERR)
        {
            nAnimFrame++;
            DrawStatusBar();
            if (GetTime() - nLastFullRedraw >= 2)
            {
                DrawContent();
                nLastFullRedraw = GetTime();
            }
            doupdate();
            continue;
        }

        if (ch == KEY_RESIZE)
        {
            DestroyWindows();
            CreateWindows();
            DrawAll();
            continue;
        }

        if (ch == 'q' || ch == 'Q')
            break;

        if (ch >= '1' && ch <= '5')
        {
            SwitchTab(ch - '1');
            DrawAll();
            continue;
        }

        if (nCurrentTab == TAB_SEND)
        {
            HandleSendInput(ch);
            DrawContent();
            DrawHelpBar();
            doupdate();
            continue;
        }

        if (ch == KEY_UP || ch == 'k')
        {
            if (nScrollOffset > 0) nScrollOffset--;
            DrawContent();
            doupdate();
            continue;
        }
        if (ch == KEY_DOWN || ch == 'j')
        {
            if (nScrollOffset < nContentLines - 1) nScrollOffset++;
            DrawContent();
            doupdate();
            continue;
        }
        if (ch == KEY_PPAGE)
        {
            int r, c;
            getmaxyx(winContent, r, c);
            (void)c;
            nScrollOffset -= (r - 3);
            if (nScrollOffset < 0) nScrollOffset = 0;
            DrawContent();
            doupdate();
            continue;
        }
        if (ch == KEY_NPAGE)
        {
            int r, c;
            getmaxyx(winContent, r, c);
            (void)c;
            nScrollOffset += (r - 3);
            if (nScrollOffset >= nContentLines) nScrollOffset = max(0, nContentLines - 1);
            DrawContent();
            doupdate();
            continue;
        }

        if (ch == 'r' || ch == 'R')
        {
            nScrollOffset = 0;
            DrawAll();
            continue;
        }
    }

    // Shutdown
    endwin();

    // Restore stdout for shutdown messages
    int tty = open("/dev/tty", O_WRONLY);
    if (tty >= 0)
    {
        dup2(tty, STDOUT_FILENO);
        dup2(tty, STDERR_FILENO);
        close(tty);
    }

    printf("Shutting down...\n");
    fShutdown = true;
    StopNode();
    DBFlush(true);
    printf("Bcash stopped.\n");
    return 0;
}
