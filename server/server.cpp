#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <fcntl.h>
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
void string_processing(string str)
{
    vector<int> digits;
    for (string::iterator it = str.begin (); it != str.end (); ++it )
    {
        if(isdigit(*it))
        {
            char s = *it;
            digits.push_back(atoi(&s));
        }
    }
    if(digits.size()>0)
    {
        sort(digits.rbegin(), digits.rend());
        std::ostream_iterator<int> out(cout," ");
        cout << "Digits in descending order: "; copy( digits.begin(), digits.end(), out); cout << endl;
        cout << "Sum: " << accumulate(digits.begin(), digits.end(), 0) << endl;
        cout << "Max: "<< *max_element(digits.begin(), digits.end()) << endl;
        cout << "Min: "<< *min_element(digits.begin(), digits.end()) << endl;
    }
    else
    {
        cout << "There is no digits in incoming message" << endl;
    }

}
int set_non_block_mode(int s)
{
    int fl = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}
void UDP_send(string str, int UDPsock,  struct sockaddr_in addr)
{
    char sendbuf[100] = {0};
    char recvbuf[100] = {0};

    socklen_t addrlen = sizeof(addr);
    struct timeval tv = {1, 0};
    fd_set fds;

    vector<string> to_send;
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
        cout << "Error while sending: client not respond" << endl;
        return;
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

}
int UDP_recv(int UDP_ls,  struct sockaddr_in UDP_listen_struct)
{
    string msg;
    int flags = MSG_NOSIGNAL;
    char recvbuffer[128] = { 0 };
    socklen_t addrlen = sizeof(UDP_listen_struct);

    //receiving message size
    int rcv = recvfrom(UDP_ls, recvbuffer, sizeof(recvbuffer), 0, (struct sockaddr*)&UDP_listen_struct, &addrlen);

    if (rcv > 0)
    {
        int s_recv = 0;
        int s_all=atoi(recvbuffer);

        //sending a transmission start signal
        sendto(UDP_ls, "ok", 2, flags, (struct sockaddr*)&UDP_listen_struct, addrlen);
        int failscount = 0;

        while(s_recv<s_all)
        {
            memset(recvbuffer,'\0',sizeof(recvbuffer));

            struct timeval tv = {1, 0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(UDP_ls, &fds);

            //waiting for the incoming datagram for one second
            int res = select(UDP_ls + 1, &fds, 0, 0, &tv);

            if (res > 0) //if the datagram is received
            {
                int rcv = recvfrom(UDP_ls, recvbuffer, sizeof(recvbuffer), 0, (struct sockaddr*)&UDP_listen_struct, &addrlen);

                //parsing a datagram, getting a number
                string msgbuffer = recvbuffer; //null-terminated
                int recvnum = atoi(msgbuffer.substr(0,msgbuffer.find("_",0)).c_str());
                msgbuffer = msgbuffer.substr(msgbuffer.find("_",0)+1,msgbuffer.length());

                if(recvnum==s_recv)
                {
                    //if the datagram number is correct, adding the received fragment to the message
                    msg=msg+msgbuffer;
                    s_recv++;
                    char sendbuffer[128] = {0};
                    sprintf(sendbuffer,"%d",s_recv);

                    //sending request for the next fragment
                    sendto(UDP_ls, sendbuffer, strlen(sendbuffer), flags, (struct sockaddr*)&UDP_listen_struct, addrlen);
                    failscount = 0;
                }
                else
                {
                    //if the datagram number is not correct, request to resend a fragment
                    char sendbuffer[128] = {0};
                    sprintf(sendbuffer,"%d",s_recv);
                    sendto(UDP_ls, sendbuffer, strlen(sendbuffer), flags, (struct sockaddr*)&UDP_listen_struct, addrlen);
                    failscount++;
                }
            }
            if(failscount>10) //issuing an error if the wrong fragment is received more than 10 times
            {
                cout << "Error getting message via UDP" << endl;
                return 1;
            }
        }
    }
    cout << "Recieved message:"<< msg << endl;

    //sending a message back to the client
    UDP_send(msg,UDP_ls,UDP_listen_struct);

    //processing digits in recieved string
    string_processing(msg);

    if(msg=="exit")
        return 5;
    else
        return 0;
}
int TCP_recv_send(int TCP_ls, struct sockaddr_in TCP_listen_struct)
{
    string msg;
    int TCP_ws;
    socklen_t addrlen = sizeof(TCP_listen_struct);

    //accepting incoming connection from client

    if((TCP_ws = accept(TCP_ls, (struct sockaddr*)&TCP_listen_struct, &addrlen))<0)
    {
        cout << "TCP accepting connection error" << endl;
        return 1;
    }

    //receiving message from client
    int rcv=0;
    do {
        char buffer[100] = {0};

        //waiting for the incoming packet for 1 second
        struct timeval tv = {1, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(TCP_ws, &fds);
        int res = select(TCP_ws + 1, &fds, 0, 0, &tv);

        if(res>0)
        {
            rcv = recv(TCP_ws, buffer, sizeof(buffer), 0);
            if(rcv> sizeof(buffer))
                msg = msg + string(buffer, sizeof(buffer));
            else
                msg = msg + string(buffer);
        }
        else
        {
            break;
        }
    } while (rcv > 0);
    cout << "Recieved message:"<< msg << endl;

    //sending a message back to the client
    int size = msg.length();
    int sent = 0;
    int flags = MSG_NOSIGNAL;
    while (sent < size)
    {
        int res = send(TCP_ws, msg.c_str() + sent, size - sent, flags);
        if (res < 0)
        {
            cout << "Error sending TCP packet to client" << endl;
            break;
        }
        sent += res;
    }
    close(TCP_ws);

    //processing digits in recieved message
    string_processing(msg);

    if(msg=="exit")
        return 5;
    else
        return 0;
}

int main(int argc, char * argv[])
{
    if(argc<3)
    {
        cout << "Usage: TCP_listner_port UDP_listener_port" << endl;
        return 2;
    }
    int TCP_listen;
    int UDP_listen;
    int TCP_in=atoi(argv[1]);
    int UDP_in=atoi(argv[2]);

    struct sockaddr_in TCP_listen_struct;
    struct sockaddr_in UDP_listen_struct;

    //listening sockets initialization

    if((TCP_listen = socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        cout << "TCP socket creating error" << endl;
        return 2;
    }

    set_non_block_mode(TCP_listen);

    memset(&TCP_listen_struct, 0, sizeof(TCP_listen_struct));
    TCP_listen_struct.sin_family = AF_INET;
    TCP_listen_struct.sin_port = htons(TCP_in);
    TCP_listen_struct.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(TCP_listen, (struct sockaddr*) &TCP_listen_struct, sizeof(TCP_listen_struct)) < 0)
    {
        cout << "TCP bind error" << endl;
        return 2;
    }

    if (listen(TCP_listen, 1) < 0)
    {
        cout << "TCP listening error" << endl;
        return 2;
    }

    UDP_listen = socket(AF_INET, SOCK_DGRAM, 0);
    if (UDP_listen < 0)
    {
        cout << "UDP listening error" << endl;
        return 2;
    }
    set_non_block_mode(UDP_listen);

    memset(&UDP_listen_struct, 0, sizeof(UDP_listen_struct));
    UDP_listen_struct.sin_family = AF_INET;
    UDP_listen_struct.sin_port = htons(UDP_in);
    UDP_listen_struct.sin_addr.s_addr = htonl(INADDR_ANY);


    if (bind(UDP_listen, (struct sockaddr*) &UDP_listen_struct, sizeof(UDP_listen_struct)) < 0)
    {
        cout << "UDP bind error" << endl;
        return 2;
    }

    //parallel service mechanism initialization

    struct pollfd pfd[1];
    pfd[0].fd = TCP_listen;
    pfd[1].fd = UDP_listen;
    pfd[0].events = pfd[1].events = POLLIN;
    pfd[0].revents = pfd[1].revents = 0;

    while (1)
    {
        int ev_cnt = poll(pfd, 2, 10);
        if (ev_cnt > 0)
        {
            if (pfd[0].revents & POLLIN)
            {
                if(TCP_recv_send(pfd[0].fd,TCP_listen_struct)==5)
                {
                    cout << "Shutdown command received " << endl;
                    break;
                }
            }
            if (pfd[1].revents)
            {
                if(UDP_recv(pfd[1].fd,UDP_listen_struct)==5)
                {
                    cout << "Shutdown command received " << endl;
                    break;
                }
            }
        }
    }
    close(TCP_listen);
    close(UDP_listen);
    return 0;
}