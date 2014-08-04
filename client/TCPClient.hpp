#ifndef TCPCLIENT_HPP__
#define TCPCLIENT_HPP__

#include<iostream>    //cout
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<string>  //string
#include <vector>
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<netdb.h> //hostent
#include <unistd.h>

#define RECEIVE_BUFFER_SIZE 1024

void *receiver(void * v);

enum read_modes {
    INCOME, BUFFERSIZE
};

class TCPClient
{
private:
    int sock;
    std::string server_address;
    int destination_port;
    struct sockaddr_in server;
    pthread_t receive;
    void conn();
    read_modes read_mode;
public:
    TCPClient(std::string address, int port);
    ~TCPClient();
    void send_data(std::string data);
    std::string receive_data(int size);
    void set_read_mode(read_modes mode);
    read_modes get_read_mode();
    int get_sock();
    void close_conn();
};

#endif