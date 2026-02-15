// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"

#include <netdb.h>


#pragma pack(1)
struct ircaddr
{
    int ip;
    short port;
};

string EncodeAddress(const CAddress& addr)
{
    struct ircaddr tmp;
    tmp.ip    = addr.ip;
    tmp.port  = addr.port;

    vector<unsigned char> vch(UBEGIN(tmp), UEND(tmp));
    return string("u") + EncodeBase58Check(vch);
}

bool DecodeAddress(string str, CAddress& addr)
{
    vector<unsigned char> vch;
    if (!DecodeBase58Check(str.substr(1), vch))
        return false;

    struct ircaddr tmp;
    if (vch.size() != sizeof(tmp))
        return false;
    memcpy(&tmp, &vch[0], sizeof(tmp));

    addr  = CAddress(tmp.ip, tmp.port);
    return true;
}






static bool Send(SOCKET hSocket, const char* pszSend)
{
    if (strstr(pszSend, "PONG") != pszSend)
        printf("SENDING: %s\n", pszSend);
    const char* psz = pszSend;
    const char* pszEnd = psz + strlen(psz);
    while (psz < pszEnd)
    {
        int ret = send(hSocket, psz, pszEnd - psz, 0);
        if (ret < 0)
            return false;
        psz += ret;
    }
    return true;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    loop
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
        }
        else if (nBytes <= 0)
        {
            if (!strLine.empty())
                return true;
            // socket closed
            printf("IRC socket closed\n");
            return false;
        }
        else
        {
            // socket error
            int nErr = errno;
            if (nErr != EMSGSIZE && nErr != EINTR && nErr != EINPROGRESS)
            {
                printf("IRC recv failed: %d\n", nErr);
                return false;
            }
        }
    }
}

bool RecvLineIRC(SOCKET hSocket, string& strLine)
{
    loop
    {
        bool fRet = RecvLine(hSocket, strLine);
        if (fRet)
        {
            if (fShutdown)
                return false;
            vector<string> vWords;
            ParseString(strLine, ' ', vWords);
            if (vWords[0] == "PING")
            {
                strLine[1] = 'O';
                strLine += '\r';
                Send(hSocket, strLine.c_str());
                continue;
            }
        }
        return fRet;
    }
}

bool RecvUntil(SOCKET hSocket, const char* psz1, const char* psz2=NULL, const char* psz3=NULL)
{
    loop
    {
        string strLine;
        if (!RecvLineIRC(hSocket, strLine))
            return false;
        printf("IRC %s\n", strLine.c_str());
        if (psz1 && strLine.find(psz1) != -1)
            return true;
        if (psz2 && strLine.find(psz2) != -1)
            return true;
        if (psz3 && strLine.find(psz3) != -1)
            return true;
    }
}




bool fRestartIRCSeed = false;

// IRC servers to try for peer discovery
static const struct {
    const char* hostname;
    const char* port;
} irc_servers[] = {
    { "irc.libera.chat", "6667" },
    { "chat.freenode.net", "6667" },
};
static const int nIRCServers = sizeof(irc_servers) / sizeof(irc_servers[0]);

void ThreadIRCSeed(void* parg)
{
    int nRetryDelay = 10; // seconds
    loop
    {
        // Try each IRC server in order
        bool fConnected = false;
        SOCKET hSocket = INVALID_SOCKET;
        for (int s = 0; s < nIRCServers && !fConnected; s++)
        {
            printf("IRC: resolving %s\n", irc_servers[s].hostname);

            struct addrinfo hints, *res = NULL;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            int err = getaddrinfo(irc_servers[s].hostname, irc_servers[s].port, &hints, &res);
            if (err != 0 || res == NULL)
            {
                printf("IRC: failed to resolve %s: %s\n", irc_servers[s].hostname, gai_strerror(err));
                if (res) freeaddrinfo(res);
                continue;
            }

            struct sockaddr_in* sinp = (struct sockaddr_in*)res->ai_addr;
            CAddress addrConnect(sinp->sin_addr.s_addr, sinp->sin_port);
            freeaddrinfo(res);

            if (!ConnectSocket(addrConnect, hSocket))
            {
                printf("IRC: connect to %s failed\n", irc_servers[s].hostname);
                continue;
            }
            printf("IRC: connected to %s\n", irc_servers[s].hostname);
            fConnected = true;
        }

        if (!fConnected)
        {
            printf("IRC: all servers failed\n");
            goto retry;
        }

        if (!RecvUntil(hSocket, "Found your hostname", "using your IP address instead", "Couldn't look up your hostname"))
        {
            closesocket(hSocket);
            goto retry;
        }

        {
        string strMyName = EncodeAddress(addrLocalHost);

        if (!addrLocalHost.IsRoutable())
            strMyName = strprintf("x%u", GetRand(1000000000));

        Send(hSocket, strprintf("NICK %s\r", strMyName.c_str()).c_str());
        Send(hSocket, strprintf("USER %s 8 * : %s\r", strMyName.c_str(), strMyName.c_str()).c_str());

        if (!RecvUntil(hSocket, " 004 "))
        {
            closesocket(hSocket);
            goto retry;
        }
        Sleep(500);

        Send(hSocket, "JOIN #bnet\r");
        Send(hSocket, "WHO #bnet\r");

        while (!fRestartIRCSeed)
        {
            string strLine;
            if (fShutdown || !RecvLineIRC(hSocket, strLine))
            {
                closesocket(hSocket);
                goto retry;
            }
            if (strLine.empty() || strLine[0] != ':')
                continue;
            printf("IRC %s\n", strLine.c_str());

            vector<string> vWords;
            ParseString(strLine, ' ', vWords);
            if (vWords.size() < 2)
                continue;

            char pszName[512];
            pszName[0] = '\0';

            if (vWords[1] == "352" && vWords.size() >= 8)
            {
                // index 7 is limited to 16 characters
                // could get full length name at index 10, but would be different from join messages
                strncpy(pszName, vWords[7].c_str(), sizeof(pszName)-1);
                pszName[sizeof(pszName)-1] = '\0';
                printf("GOT WHO: [%s]  ", pszName);
            }

            if (vWords[1] == "JOIN")
            {
                // :username!username@50000007.F000000B.90000002.IP JOIN :#channelname
                strncpy(pszName, vWords[0].c_str() + 1, sizeof(pszName)-1);
                pszName[sizeof(pszName)-1] = '\0';
                if (strchr(pszName, '!'))
                    *strchr(pszName, '!') = '\0';
                printf("GOT JOIN: [%s]  ", pszName);
            }

            if (pszName[0] == 'u')
            {
                CAddress addr;
                if (DecodeAddress(pszName, addr))
                {
                    CAddrDB addrdb;
                    if (AddAddress(addrdb, addr))
                        printf("new  ");
                    addr.print();
                }
                else
                {
                    printf("decode failed\n");
                }
            }
        }

        fRestartIRCSeed = false;
        closesocket(hSocket);
        }

    retry:
        printf("IRC: retrying in %d seconds\n", nRetryDelay);
        Sleep(nRetryDelay * 1000);
        if (nRetryDelay < 600)
            nRetryDelay *= 2;
    }
}










#ifdef TEST
int main(int argc, char *argv[])
{
#ifdef _WIN32
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,2), &wsadata) != NO_ERROR)
    {
        printf("Error at WSAStartup()\n");
        return false;
    }
#endif

    ThreadIRCSeed(NULL);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
#endif
