#ifndef TCPCLIENT_HPP__
#define TCPCLIENT_HPP__

#include<iostream>    //cout
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<string>  //string
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<netdb.h> //hostent

#define RECEIVE_BUFFER_SIZE 7

void *receiver(void * v);

class TCPClient
{
private:
    std::string server_address;
    int destination_port;
    struct sockaddr_in server;
    pthread_t receive;
    void conn();
public:
    int sock;
    TCPClient(std::string address, int port);
    ~TCPClient();
    void send_data(std::string data);
    std::string receive_data(int size);
};

#endif