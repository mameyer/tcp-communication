#include "TCPServer.hpp"
#include "../common/common.hpp"

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

#define READ_BUFFER_SIZE 250
#define MAX_CON 30

/**
 * constructor for TCPServer.
 *
 * @param address specifies the address the server is running on.
 * @param port the port the server listens on.
 */
TCPServer::TCPServer(std::string address, int port) : threads_to_join(0), access_connections(1), select_client(0), connections_to_userspace(true), cmds_to_execute(0), sd(-1) {
    this->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (this->sd < 0)
    {
        perror("could not create socket");
        return;
    }

    this->server_mode = STARTING;

    std::cout << "sd: " << this->sd << std::endl;

    this->addr.sin_family = AF_INET;
    this->addr.sin_port = htons(port);
    this->addr.sin_addr.s_addr = INADDR_ANY;

    this->addrlen = sizeof(this->addr);

    bind(sd, (struct sockaddr*)&this->addr, sizeof(this->addr));

    int reuse = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        perror("setsockopt");
    }
    
    this->available_commands["print conns"] = PRINT_CONNS;
    this->available_commands["help"] = HELP;
    
    this->register_cmd_handler(UNKNOWN, this->cmd_show_help);
    this->register_cmd_handler(PRINT_CONNS, this->cmd_print_connections);
    this->register_cmd_handler(HELP, this->cmd_show_help);
    
    if (pthread_create(&this->runner, 0, accept_conn, (void *)this)) {
        perror("Failed to create thread");
    }
    
    if (pthread_create(&this->stdin_command_reader, 0, execute_cmd, (void *)this)) {
        perror("Failed to create thread");
    }
}

void
TCPServer::register_cmd_handler(enum CmdIds id, void (*handler)(void *params)) {
    this->cmdHandlers[id] = handler;
}

void *
execute_cmd(void* v)
{
    TCPServer *server = (TCPServer *) v;
    assert(server != NULL);
    
    while (server->server_mode != CLOSING) {
        server->cmds_to_execute.P();
        
        if (server->server_mode == CLOSING) {
            std::cout << "break execute_cmd" << std::endl;
            break;
        }
        
        CmdIds cmd_id = server->next_cmd_to_execute();
        
        if (cmd_id != CMD_UNAVAIL) {
            server->cmdHandlers[cmd_id](server);
        }
    }
}

void
TCPServer::parse_cmd(std::string cmd)
{
    if (!cmd.empty()) {
        CmdIds cmd_id = this->available_commands[cmd];
        
        if (cmd_id != NULL) {
            this->cmds_requested.push(cmd_id);
        }
    } else {
        this->cmds_requested.push(HELP);
    }
}

void
TCPServer::cmd_print_connections(void *params) {
    TCPServer *server = (TCPServer *) params;
    assert(server != NULL);
    
    std::map<int, pthread_t>::iterator it;
    std::map<int, pthread_t> conns = server->get_conns();

    for (it = conns.begin(); it != conns.end(); it++) {
        std::cout << it->first << std::endl;
    }
}

void
TCPServer::cmd_show_help(void *params) {
    TCPServer *server = (TCPServer *) params;
    assert(server != NULL);
    
    std::cout << "available commands:" << std::endl;
    
    std::map<std::string, enum CmdIds>::iterator it;
    for (it = server->available_commands.begin(); it != server->available_commands.end(); it++) {
        std::cout << it->first << std::endl;
    }
}

int
TCPServer::get_num_open_conns() {
    return this->connections.size();
}

/**
 * task for collector thread. joins terminated threads of closed connections.
 *
 * @param v reference to TCPServer instance.
 */
void *
collect(void * v) {
    TCPServer *server = ((TCPServer *) v);
    assert (server != NULL);

    while (1) {
        // wait for next thread to join
        server->threads_to_join.P();

        // ask join_requested queue for last closed connection
        int conn = server->next_to_join();

        if (conn >= 0) {
            // select examines all file descriptors sets whose addresses are passed
            // in the readfds; while select works with file descriptor set for conn
            // don´t close con!
            pthread_t *thread = server->find_thread(conn);

            if (pthread_join(*thread, 0)) {
                perror("Failed to join thread");
            } else {
                std::cout << "joined thread " << conn << std::endl;
            }

            server->access_connections.P();
            server->connections_to_userspace = false;
            server->erase_conn(conn);
            server->connections_to_userspace = true;
            server->access_connections.V();
            close(conn);

            if (server->server_mode == STOPPING) {
                // your job is done when all connections are closed..
                if (server->get_num_open_conns() == 0) {
                    break;
                }
            }
        } else {
            // no open connections after server status set to STOPPING
            break;
        }
    }
}

/**
 * desctrutor for TCPServer. closes open connections and joins threads.
 */
TCPServer::~TCPServer()
{
    this->server_mode = CLOSING;
    
    // join cmd reader
    // wake up thread
    if (this->cmds_requested.empty()) {
        this->cmds_to_execute.V();
    }
    
    if (this->stdin_command_reader != NULL && this->stdin_command_reader > 0 && pthread_join(this->stdin_command_reader, 0))
    {
        perror("Failed to join thread");
    } else {
        std::cout << "joined stdin_command_reader.." << std::endl;
    }
    
    // join runner thread
    if (this->runner != NULL && this->runner > 0 && pthread_join(this->runner, 0))
    {
        perror("Failed to join thread");
    } else {
        std::cout << "joined runner.." << std::endl;
    }

    if (this->sd >= 0) {
        int sd_old = this->sd;
        close(this->sd);
        std::cout << "closed socket: " << sd_old << std::endl;
    }
}

/**
 * closes all open connections. informs clients of this process.
 */
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
}

/**
 * initializes listening for incomming connections.
 *
 * @param num_conns limits the queue of incomming connections.
 */
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
    // create thread for joining listener threads (sperate thread for every connection)
    if (pthread_create(&this->collector, 0, collect, (void *)this)) {
        perror("failed to create collector thread");
    }

    init_listen(num_conns);

    this->server_mode = RUNNING;
}

void
TCPServer::stop() {
    this->server_mode = STOPPING;
    std::cout << "stop: a).." << std::endl;

    // necessary so blocked listener can move in to join queue
    this->select_client.V();

    std::cout << "stop: b).." << std::endl;
    // this->close_conns();

    // no open connections after server status set to STOPPING
    // wake up colletor thread to join
    if (this->connections.size() == 0) {
        this->threads_to_join.V();
    }

    // join colector thread
    if (this->collector != NULL && this->collector > 0 && pthread_join(this->collector, 0))
    {
        perror("Failed to join thread");
    } else {
        std::cout << "joined collector.." << std::endl;
    }
}

void *
accept_conn(void * v) {
    TCPServer *server = ((TCPServer *) v);

    assert (server != NULL);
    assert (server->sd != NULL);
    assert (server->sd >= 0);
    // assert (server->listen_active);

    int ret;
    int max_sd;
    int sfd;

    while(server->server_mode != CLOSING) {
        // clear socket set
        FD_ZERO(&server->read_flags);

        FD_SET(server->sd,&server->read_flags);
        max_sd = server->sd;
        
        // reading from STDIN
        FD_SET(STDIN_FILENO, &server->read_flags);
        max_sd++;
        sfd = max_sd;

        server->access_connections.P();
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
        server->access_connections.V();

        // 10 milliseconds using timeval for select
        struct timeval waitd;
        waitd.tv_sec = 0;
        waitd.tv_usec = 10000;

        ret = select(max_sd + 1, &server->read_flags, &server->write_flags, (fd_set*)0, &waitd);
        if (server->server_mode == CLOSING) {
            std::cout << "select break.." << std::endl;
            break;
        }

        /* check result */
        if (ret < 0) {
            // perror("select");
        } else if ((server->server_mode == RUNNING) && FD_ISSET(server->sd, &server->read_flags)) {
            /* accept connection and pass new socket to handler thread */
            int conn;
            sockaddr_storage addr;
            socklen_t socklen = sizeof(addr);

            // conn = accept4(server->sd, (sockaddr*)&addr, &socklen, SOCK_NONBLOCK);
            conn = accept(server->sd, (sockaddr*)&addr, &socklen);
            std::cout << "accept.. " << conn << std::endl;
            if (conn < 0) {
                perror("accept");
            } else {
                // set non-blocking
                // recv()/send() non blocking
                // set_nonblock(conn);

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
        } else if (FD_ISSET(STDIN_FILENO, &server->read_flags)) {
            // std::cout << "you typed: " << sfd << std::endl;
            // start thread for working with input data
            std::string line;
            std::cout << "getl read from stdin: " << line << std::endl;
            std::getline(std::cin, line);
            std::cout << "read from stdin: " << line << std::endl;
            server->parse_cmd(line);
            server->cmds_to_execute.V();
        } else {
            server->select_client.V();
        }
    }
}

void *
listener(void *v) {
    std::pair<TCPServer*, int> *context = ((std::pair<TCPServer*, int> *) v);
    int conn = context->second;
    std::cout << "listener attached to conn " << conn << std::endl;

    while(1) {
        context->first->select_client.P();

        if (context->first->server_mode == STOPPING) {
            // free sema so other listener can join, too..
            context->first->select_client.V();

            // send BYE
            std::cout << "send bye" << std::endl;
            std::string data = SERVER_BYE;
            if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                perror("send failed!\n");
            }
            std::cout << "finished sending bye" << std::endl;

            // register in join queue
            context->first->register_join_reqeust(conn);
            context->first->threads_to_join.V();

            delete context;
            break;
        }

        if (FD_ISSET(conn, &context->first->read_flags)) {
            char read_buffer[READ_BUFFER_SIZE];
            int numRead = recv(conn, read_buffer, READ_BUFFER_SIZE, 0);

            if(numRead < 0) {
                perror("data error");
            } else if (numRead == 0) {
                // client disconnect
                getpeername(conn , (struct sockaddr*)&context->first->addr , (socklen_t*)&context->first->addrlen);
                printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(context->first->addr.sin_addr) , ntohs(context->first->addr.sin_port));

                context->first->register_join_reqeust(conn);
                context->first->threads_to_join.V();

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
            context->first->select_client.V();
        }
    }

    return 0;
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

CmdIds
TCPServer::next_cmd_to_execute() {
    if (this->cmds_requested.empty()) {
        return CMD_UNAVAIL;
    }

    CmdIds cmd_id = this->cmds_requested.front();
    this->cmds_requested.pop();

    return cmd_id;
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
    while (!this->connections_to_userspace) {
        
    }
    
    return this->connections;
}
