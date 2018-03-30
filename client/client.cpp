#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iterator>

/*
 * Test task for PROTEI
 * 2018 Vladimir Eremuk
 */

using namespace std;

int UDP_recieve(int UDP_ls,  struct sockaddr_in UDP_listen_struct)
{
    string msg;
    int flags = MSG_NOSIGNAL;
    char recvbuffer[128] = { 0 };
    socklen_t addrlen = sizeof(UDP_listen_struct);

    int rcv = recvfrom(UDP_ls, recvbuffer, sizeof(recvbuffer), 0, (struct sockaddr*)&UDP_listen_struct, &addrlen);
    if (rcv > 0)
    {
        int s_all=atoi(recvbuffer); int s_recv = 0;
        sendto(UDP_ls, "ok", 2, flags, (struct sockaddr*)&UDP_listen_struct, addrlen);
        int failscount = 0;
        while(s_recv<s_all)
        {
            struct timeval tv = {1, 0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(UDP_ls, &fds);
            int res = select(UDP_ls + 1, &fds, 0, 0, &tv);
            memset(recvbuffer,'\0',sizeof(recvbuffer));
            if (res > 0)
            {
                int rcv = recvfrom(UDP_ls, recvbuffer, sizeof(recvbuffer), 0, (struct sockaddr*)&UDP_listen_struct, &addrlen);
                string msgbuffer = recvbuffer;
                int recvnum = atoi(msgbuffer.substr(0,msgbuffer.find("_",0)).c_str());
                msgbuffer = msgbuffer.substr(msgbuffer.find("_",0)+1,msgbuffer.length());
                if(recvnum==s_recv)
                {
                    msg=msg+msgbuffer;
                    s_recv++;
                    char sendbuffer[128] = {0};
                    sprintf(sendbuffer,"%d",s_recv);
                    sendto(UDP_ls, sendbuffer, strlen(sendbuffer), flags, (struct sockaddr*)&UDP_listen_struct, addrlen);
                    failscount = 0;
                }
                else
                {
                    char sendbuffer[128] = {0};
                    sprintf(sendbuffer,"%d",s_recv);
                    sendto(UDP_ls, sendbuffer, strlen(sendbuffer), flags, (struct sockaddr*)&UDP_listen_struct, addrlen);
                    failscount++;
                }
            }
            if(failscount==10)
                return 1;
        }
    }
    cout << "Recieved response:"<< msg << endl;
}
void UDP_send(char *ip, int port, int timeout)
{
    cout << "UDP mode; type 'exit' for shutdown (client+server)" << endl;
    struct sockaddr_in addr;
    int UDPsock;
    socklen_t addrlen = sizeof(addr);

    if ((UDPsock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        std::cout<<"Socket creating error"<<endl;
        return;
    }

    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_aton(ip , &addr.sin_addr) == 0)
    {
        std::cout<<"inet_aton error"<<endl;
        return;
    }

    while(1)
    {
        char sendbuf[100] = {0};
        char recvbuf[100] = {0};
        struct timeval tv = {1, 0};
        fd_set fds;
        string str;
        vector<string> to_send;

        cout<<"Enter string:"<<endl;
        getline(cin, str);

        for(int a=0, i=0; a<str.length(); a+=64,i++)
            to_send.push_back(str.substr(a,64).insert(0,to_string(i)+"_"));


        sprintf(sendbuf,"%d", to_send.size());
        sendto(UDPsock, sendbuf, strlen(sendbuf) , 0 , (struct sockaddr *) &addr, addrlen);

        FD_ZERO(&fds);
        FD_SET(UDPsock, &fds);
        int res = select(UDPsock + 1, &fds, 0, 0, &tv);
        if(res>0)
        {
            int rcv = recvfrom(UDPsock, recvbuf, 100, 0, (struct sockaddr *) &addr, &addrlen);
        }
        else
        {
            cout << "Error" << endl;
            break;
        }

        int s_count = 0; int failscount = 0;
        while(s_count<to_send.size() && failscount<10)
        {
            memset(recvbuf,'\0',sizeof(recvbuf));
            sendto(UDPsock, to_send[s_count].c_str(), to_send[s_count].length() , 0 , (struct sockaddr *) &addr, addrlen);

            FD_ZERO(&fds);
            FD_SET(UDPsock, &fds);
            int res = select(UDPsock + 1, &fds, 0, 0, &tv);
            if(res>0)
            {
                recvfrom(UDPsock, recvbuf, 100, 0, (struct sockaddr *) &addr, &addrlen);
                s_count=atoi(recvbuf);
                failscount = 0;
            }
            else
                failscount++;
        }

        tv = {timeout, 0};
        FD_ZERO(&fds);
        FD_SET(UDPsock, &fds);
        res = select(UDPsock + 1, &fds, 0, 0, &tv);
        if(res>0)
        {
            UDP_recieve(UDPsock,addr);
        }
        else
        {
            cout<<"Response not recieved"<<endl;
        }
        if(str=="exit")
            break;
    }
    close(UDPsock);
}
void TCP_send_recieve(char *ip, int port, int timeout)
{
    cout << "TCP mode; type 'exit' for shutdown (client+server)" << endl;
    while(1)
    {
        string str;
        cout<<"Enter string:"<<endl;
        getline(cin, str);
        struct sockaddr_in TCP_addr;
        struct sockaddr_in addr;
        int TCPsock;
        TCPsock = socket(AF_INET, SOCK_STREAM, 0);
        if (TCPsock < 0)
            return;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_aton(ip , &addr.sin_addr) == 0)
        {
            std::cout<<"inet_aton error"<<endl;
            return;
        }
        if (connect(TCPsock, (struct sockaddr*) &addr, sizeof(addr)) != 0)
        {
            close(TCPsock);
            return;
        }
        int flags = MSG_NOSIGNAL;
        int size = str.length();
        int sent = 0;
        while (sent < size)
        {

            int res = send(TCPsock, str.c_str() + sent, size - sent, flags);
            if (res < 0)
                return;
            sent += res;
        }
        char buffer[100];
        int rcv=0;
        string msg;
        do {
            memset(buffer,'\0',sizeof(buffer));
            struct timeval tv = {timeout, 0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(TCPsock, &fds);
            int res = select(TCPsock + 1, &fds, 0, 0, &tv);
            if(res>0)
            {
                rcv = recv(TCPsock, buffer, sizeof(buffer), 0);
                if(rcv> sizeof(buffer))
                    msg = msg + string(buffer, sizeof(buffer));
                else
                    msg = msg + string(buffer);
            }
            else
                break;
        } while (rcv > 0);
        if(msg.length()>0)
            cout << "Recieved response:"<< msg << endl;
        else
            cout<<"Response not recieved"<<endl;
        close(TCPsock);
        if(str=="exit")
            return;
    }
}
int main(int argc, char * argv[])
{
    if(argc<5)
    {
        cout << "Usage: IP port response_timeout(in seconds) mode(0-UDP,1-TCP)" << endl;
        return 2;
    }
    if(!atoi(argv[4]))
    {
        UDP_send(argv[1],atoi(argv[2]),atoi(argv[3]));
    }
    else
        TCP_send_recieve(argv[1],atoi(argv[2]),atoi(argv[3]));


    return 0;
}
