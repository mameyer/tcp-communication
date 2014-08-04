#include "TCPClient.hpp"

#include <assert.h>
#include <unistd.h>
#include <pthread.h>

bool running = true;

TCPClient::TCPClient(std::string address, int port) : server_address(address), destination_port(port), sock(-1) {
    this->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (this->sock < 0)
    {
        perror("could not create socket");
        return;
    }

    std::cout << "socket created: " << this->sock << std::endl;
    
    this->conn();

    if(pthread_create(&this->receive, 0, receiver, (void *)this))
    {
        perror("Failed to create thread");
    }
}

TCPClient::~TCPClient()
{
    running = false;

    if (this->sock >= 0) {
        close(this->sock);
    }

    if (this->receive != NULL && this->receive > 0 && pthread_join(this->receive, 0))
    {
        perror("Failed to join thread");
    }
}

void
TCPClient::conn() {
    assert (this->sock >= 0);

    //setup address structure
    if(inet_addr(this->server_address.c_str()) == -1)
    {
        struct hostent *he;
        struct in_addr **addr_list;

        //resolve the hostname, its not an ip address
        if ((he = gethostbyname( this->server_address.c_str())) == NULL)
        {
            //gethostbyname failed
            herror("gethostbyname");
            std::cout << "Failed to resolve hostname" << std::endl;

            return;
        }

        //Cast the h_addr_list to in_addr , since h_addr_list also has the ip address in long format only
        addr_list = (struct in_addr **) he->h_addr_list;

        for(int i = 0; addr_list[i] != NULL; i++)
        {
            this->server.sin_addr = *addr_list[i];
            std::cout << this->server_address << " resolved to "<<inet_ntoa(*addr_list[i]) << std::endl;

            break;
        }
    }

    //plain ip address
    else
    {
        this->server.sin_addr.s_addr = inet_addr(this->server_address.c_str());
    }

    this->server.sin_family = AF_INET;
    this->server.sin_port = htons(this->destination_port);

    //Connect to remote server
    if (connect(this->sock , (struct sockaddr *)&this->server , sizeof(this->server)) < 0)
    {
        perror("connect failed. Error");
        return;
    }

    std::cout << "Connected" << std::endl;
}

void
TCPClient::send_data(std::string data) {
    assert (data.size() > 0);
    assert (this->sock >= 0);

    int allBytesSent = 0;
    int actBytesSent = 0;

    while (allBytesSent < data.size()) {
        actBytesSent = send(this->sock, data.c_str(), strlen(data.c_str()), 0);
        
        if (actBytesSent < 0) {
            perror("send failed!\n");
            close(this->sock);
            running = false;
            return;
        }
        
        allBytesSent += actBytesSent;
    }

    std::cout << "data sent!" << std::endl;
}

void *
receiver(void * v) {
    TCPClient *client = (TCPClient*) v;

    assert(client != NULL);
    assert (client->sock >= 0);

    while (running) {
        char buffer[RECEIVE_BUFFER_SIZE];
        int allBytesRead = 0;
        int actBytesRead = 0;

        while (allBytesRead < (RECEIVE_BUFFER_SIZE-1)) {
            actBytesRead = recv(client->sock, buffer, sizeof(buffer), 0);
            
            if (actBytesRead < 0) {
                perror("receive failed!");
                break;
            } else if (actBytesRead == 0) {
                 std::cout << "connection reset by peer.. " << buffer << std::endl;
                 close(client->sock);
                 running = false;
                 return 0;
            }

            allBytesRead += actBytesRead;
        }
        
        buffer[allBytesRead] = '\0';
        std::cout << "received.. " << buffer << std::endl;
    }

    return 0;
}