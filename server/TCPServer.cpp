#include "TCPServer.hpp"

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <istream>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "../common/Sema.hh"

#define READ_BUFFER_SIZE 250
#define MAX_CON 30

Sema threads_to_join(0);
Sema access_connections(1);
Sema select_client(0);

bool running = true;

TCPServer::TCPServer(std::string address, int port) : sd(-1) {
    this->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (this->sd < 0)
    {
        perror("could not create socket");
        return;
    }

    std::cout << "sd: " << this->sd << std::endl;

    this->addr.sin_family = AF_INET;
    this->addr.sin_port = htons(port);
    this->addr.sin_addr.s_addr = INADDR_ANY;
    
    this->addrlen = sizeof(this->addr);

    bind(sd, (struct sockaddr*)&this->addr, sizeof(this->addr));

    int yes = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt");
    }

    if (pthread_create(&this->collector, 0, collect, (void *)this)) {
        perror("Failed to create thread");
    }
}

void *
collect(void * v) {
    while (1) {
        threads_to_join.P();

        TCPServer *server = ((TCPServer *) v);
        int conn = server->next_to_join();

        if (conn >= 0) {
            access_connections.P();
            pthread_t *thread = server->find_thread(conn);

            if (pthread_join(*thread, 0)) {
                perror("Failed to join thread");
            } else {
                std::cout << "joined thread " << conn << std::endl;
            }

            server->erase_conn(conn);
            access_connections.V();
        } else {
            std::cout << "no next_to_join.." << std::endl;
            if (!running) break;
        }
    }
}

TCPServer::~TCPServer()
{
    this->close_conns();

    if (this->collector != NULL && this->collector > 0 && pthread_join(this->collector, 0))
    {
        perror("Failed to join thread");
    }
}

void
TCPServer::close_conns() {
    if (this->connections.size()>0) {
        std::map<int, pthread_t>::iterator it = this->connections.begin();

        while (it != this->connections.end()) {
            std::map<int, pthread_t>::iterator toErase = it;
            it++;

            int conn = toErase->first;
            if (conn >=0) {
                // send BYE
                std::cout << "send bye" << std::endl;
                std::string data = SERVER_BYE;
                if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                    perror("send failed!\n");
                }
                std::cout << "finished sending bye" << std::endl;
                
                close(conn);
                std::cout << "closed conn: " << conn << std:: endl;
            }

            this->connections.erase(toErase);
        }
    }

    if (this->sd >= 0) {
        close(sd);
    }
}

void
TCPServer::init_listen(int num_conns) {
    assert (this->sd >= 0);

    if (listen(this->sd, num_conns) < 0) {
        perror("listen");
    }

    this->listen_active = true;
}

void
TCPServer::run(int num_conns) {
    init_listen(num_conns);
    
    if (pthread_create(&this->runner, 0, accept_conn, (void *)this)) {
        perror("Failed to create thread");
    }
}

void
TCPServer::stop() {
    running = false;
    threads_to_join.V();
}

void *
accept_conn(void * v) {
    TCPServer *server = ((TCPServer *) v);

    assert (server->sd != NULL);
    assert (server->sd >= 0);
    assert (server->listen_active);

    int ret;
    int max_sd;

    while(running) {
        // clear socket set
        FD_ZERO(&server->read_flags);

        FD_SET(server->sd,&server->read_flags);
        max_sd = server->sd;

        /*
        FD_ZERO(&server->write_flags);
        FD_SET(server->sd,&server->write_flags);

        FD_SET(STDIN_FILENO, &server->read_flags);
        FD_SET(STDIN_FILENO, &server->write_flags);
        */

        // add child sockets to set
        
        access_connections.P();
        std::map<int, pthread_t>::iterator it = server->connections.begin();
        while (it != server->connections.end()) {
            int conn = it->first;

            if (conn > 0) {
                FD_SET(conn, &server->read_flags);
            }

            if (conn > max_sd) {
                max_sd = conn;
            }

            it++;
        }
        access_connections.V();

        // 10 milliseconds using timeval for select
        struct timeval waitd;
        waitd.tv_sec = 0;
        waitd.tv_usec = 10000;
        //ret = select(max_sd + 1, &server->read_flags, &server->write_flags, (fd_set*)0, &waitd);
        
        ret = select(max_sd + 1, &server->read_flags, &server->write_flags, (fd_set*)0, NULL);

        /* check result */
        if (ret < 0) {
            perror("select");
        } else if (FD_ISSET(server->sd, &server->read_flags)) {
            /* accept connection and pass new socket to handler thread */
            int conn;
            sockaddr_storage addr;
            socklen_t socklen = sizeof(addr);

            conn = accept4(server->sd, (sockaddr*)&addr, &socklen, SOCK_NONBLOCK);
            std::cout << "accept.. " << conn << std::endl;
            if (conn < 0) {
                perror("accept");
            } else {
                // set non-blocking
                // recv()/send() non blocking
                server->set_nonblock(conn);

                server->connections[conn] = NULL;
                std::cout << "con: " << conn << std::endl;

                std::pair<TCPServer*, int> *listenerContext = new std::pair<TCPServer*, int>();
                listenerContext->first = server;
                listenerContext->second = conn;

                if(pthread_create(&server->connections[conn], 0, listener, (void *)listenerContext))
                {
                    perror("Failed to create thread");
                    std::cout << conn << std::endl;
                }

                // send connection established to new client..
                std::cout << "send connection established.." << std::endl;
                std::string data = SERVER_CON_ESTABLISHED;
                if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                    perror("send failed!\n");
                }
                std::cout << "finished sending connection established.." << std::endl;
            }
        } else {
            select_client.V();
        }
    }
}

void *
listener(void *v) {
    std::pair<TCPServer*, int> *context = ((std::pair<TCPServer*, int> *) v);
    int conn = context->second;
    std::cout << "listener attached to conn " << conn << std::endl;

    while(1) {
        select_client.P();
        if (FD_ISSET(conn, &context->first->read_flags)) {
            char read_buffer[READ_BUFFER_SIZE];
            int numRead = recv(conn, read_buffer, READ_BUFFER_SIZE, 0);

            if(numRead < 0) {
                //perror("data error");
            } else if (numRead == 0) {
                // client disconnect
                getpeername(conn , (struct sockaddr*)&context->first->addr , (socklen_t*)&context->first->addrlen);
                printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(context->first->addr.sin_addr) , ntohs(context->first->addr.sin_port));
                
                context->first->register_join_reqeust(conn);
                threads_to_join.V();

                delete context;
                break;
            } else {
                std::string msg = read_buffer;
                std::cout << msg << std::endl;

                // send ack
                std::cout << "send ack" << std::endl;
                std::string data = SERVER_RECEIVED;
                if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                    perror("send failed!\n");
                }
                std::cout << "finished sending ack" << std::endl;
            }
        } else {
            select_client.V();
        }
    }

    return 0;
}

void *
reader(void *v) {

}

void *
writer(void *v) {

}

int
TCPServer::get_sd() {
    return this->sd;
}

void
TCPServer::register_join_reqeust(int conn) {
    this->join_requested.push(conn);
    std::cout << "register thread with conn " << conn << std::endl;
}

int
TCPServer::next_to_join() {
    if (this->join_requested.empty()) {
        return -1;
    }

    int conn = this->join_requested.front();
    this->join_requested.pop();

    return conn;
}

pthread_t *
TCPServer::find_thread(int conn) {
    return &this->connections[conn];
}

bool
TCPServer::erase_conn(int conn) {
    std::map<int, pthread_t>::iterator it = this->connections.find(conn);

    if (it != this->connections.end()) {
        int size_before = this->connections.size();
        this->connections.erase(it);
        if (size_before > this->connections.size()) {
            std::cout << "erased conn " << conn << std::endl;
            return true;
        }
    }

    return false;
}

std::map<int, pthread_t>
TCPServer::get_conns() {
    return this->connections;
}

// http://stackoverflow.com/questions/6715736/using-select-for-non-blocking-sockets
void
TCPServer::set_nonblock(int socket)
{
    int flags;
    flags = fcntl(socket,F_GETFL,0);
    assert(flags != -1);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}