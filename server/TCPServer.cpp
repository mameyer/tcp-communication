#include "TCPServer.hpp"
#include "../common/common.hpp"

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <istream>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sstream>

#define READ_BUFFER_SIZE 250
#define MAX_CON 30

/**
 * constructor for TCPServer.
 *
 * @param address specifies the address the server is running on.
 * @param port the port the server listens on.
 */
TCPServer::TCPServer(std::string address, int port) : threads_to_join(0), access_connections(1), select_client(0), connections_to_userspace(true), cmds_to_execute(0), sd(-1) {
    std::cout << "starting server.." << std::endl;

    this->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (this->sd < 0)
    {
        perror("!could not create socket");
        return;
    }

    this->server_mode = STARTING;

    std::cout << "server socket file descriptor is: " << this->sd << std::endl;

    this->addr.sin_family = AF_INET;
    this->addr.sin_port = htons(port);
    this->addr.sin_addr.s_addr = INADDR_ANY;

    this->addrlen = sizeof(this->addr);

    if (bind(sd, (struct sockaddr*)&this->addr, sizeof(this->addr)) < 0) {
        std::cout << "!failed to bind server port" << this->sd << std::endl;
        return;
    }

    std::cout << "server bound to adress '" << address << "' and port '" << port << "'" << std::endl;

    int reuse = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
        perror("!setsockopt");
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(this->sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    this->available_commands["conns"] = CMD_PRINT_CONNS;
    this->available_commands["help"] = CMD_HELP;
    this->available_commands["stop"] = CMD_STOP;
    this->available_commands["run"] = CMD_RUN;
    this->available_commands["exit"] = CMD_EXIT;

    this->cmdHandlers[CMD_UNKNOWN] = &TCPServer::cmd_show_help;
    this->cmdHandlers[CMD_PRINT_CONNS] = &TCPServer::cmd_print_connections;
    this->cmdHandlers[CMD_HELP] = &TCPServer::cmd_show_help;
    this->cmdHandlers[CMD_RUN] = &TCPServer::run;
    this->cmdHandlers[CMD_STOP] = &TCPServer::stop;
    this->cmdHandlers[CMD_EXIT] = &TCPServer::cmd_exit;
    
    std::cout << "server mode running is: " << RUNNING << std::endl;
    std::cout << "sever mode stopping is: " << STOPPING << std::endl;


    if (pthread_create(&this->runner, 0, accept_conn, (void *)this) == 0) {
        std::cout << "runner thread created.." << std::endl;
    } else {
        perror("!failed to create runner thread"); 
    }

    if (pthread_create(&this->stdin_command_reader, 0, execute_cmd, (void *)this) == 0) {
        std::cout << "stdin_command_reader thread created.." << std::endl;
    } else {
        perror("!failed to create stdin_command_reader thread");
    }

    std::cout << "server successfully started.." << std::endl;
    std::cout << std::endl;

    // signal user that server is ready to read cmd
    std::cout << "[server] > ";
    std::cout.flush();
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

        Cmd *cmd = server->next_cmd_to_execute();

        if ((cmd != NULL) && (cmd->id != CMD_UNAVAIL)) {
            (server->*(server->cmdHandlers[cmd->id]))(cmd);
            delete cmd;

            // signal user that server is ready to read cmd
            std::cout << "[server] > ";
            std::cout.flush();
        }
    }
}

void
TCPServer::parse_cmd(std::string cmd)
{
    if (!cmd.empty()) {
        std::istringstream iss(cmd);
        std::string cmd_id;
        iss >> cmd_id;

        std::vector<std::string> params;
        for (std::string token; iss >> token;) {
            std::cout << token << std::endl;
            params.push_back(token);
        }

        if (!cmd_id.empty()) {
            std::map<std::string, CmdIds>::iterator it = this->available_commands.find(cmd_id);
            if (it != this->available_commands.end()) {
                this->cmds_requested.push(new Cmd(it->second, params));
            } else {
                this->cmds_requested.push(new Cmd(CMD_HELP));
            }
        }
    } else {
        this->cmds_requested.push(new Cmd(CMD_HELP));
    }
}

void
TCPServer::cmd_print_connections(void *params) {
    /*if (params != NULL) {
        Cmd *cmd = (Cmd *) params;

        std::vector<std::string> infos = cmd->params;
        std::vector<std::string>::iterator it;

        for (it = infos.begin(); it != infos.end(); it++) {
            std::cout << *it << std::endl;
        }
    }*/

    std::cout << "*** CONNS" << std::endl;

    std::map<int, pthread_t>::iterator it;
    std::map<int, pthread_t> conns = this->get_conns();

    it = conns.begin();

    if (it == conns.end()) {
        std::cout << "no open conns" << std::endl;
    } else {
        for (; it != conns.end(); it++) {
            std::cout << it->first << std::endl;
        }
    }
}

void
TCPServer::cmd_exit(void *params) {
    if (this->server_mode == RUNNING) {
        this->stop((void *) 0);
    }

    if (this->exit_extern != NULL) {
        this->exit_extern->V();
    }
}

void
TCPServer::cmd_show_help(void *params) {
    std::cout << "*** HELP: availabe commands are" << std::endl;

    std::map<std::string, enum CmdIds>::iterator it;
    for (it = this->available_commands.begin(); it != this->available_commands.end(); it++) {
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

        if (server->server_mode == STOPPING) {
            // your job is done when all connections are closed..
            if (server->get_num_open_conns() == 0) {
                break;
            }
        }

        // ask join_requested queue for last closed connection
        int conn = server->next_to_join();

        if (conn >= 0) {
            // select examines all file descriptors sets whose addresses are passed
            // in the readfds; while select works with file descriptor set for conn
            // don´t close con!
            pthread_t *thread = server->find_thread(conn);

            std::cout << std::endl;
            if (pthread_join(*thread, 0) == 0) {
                std::cout << "joined client thread " << conn << std::endl;
            } else {
                perror("Failed to join thread");
            }

            server->access_connections.P();
            server->connections_to_userspace = false;
            server->erase_conn(conn);
            server->connections_to_userspace = true;
            server->access_connections.V();
            close(conn);

            // signal user that server is ready to read cmd
            std::cout << "[server] > ";
            std::cout.flush();

            if (server->server_mode == STOPPING) {
                // your job is done when all connections are closed..
                if (server->get_num_open_conns() == 0) {
                    break;
                }
            }
        }
    }
}

/**
 * desctrutor for TCPServer. closes open connections and joins threads.
 */
TCPServer::~TCPServer()
{
    this->server_mode = CLOSING;
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "closing server.." << std::endl;

    // join cmd reader
    // wake up thread
    if (this->cmds_requested.empty()) {
        this->cmds_to_execute.V();
    }

    if (this->stdin_command_reader != NULL && this->stdin_command_reader > 0)
    {
        if (pthread_join(this->stdin_command_reader, 0) == 0) {
            std::cout << "joined stdin_command_reader thread.." << std::endl;
        } else {
            perror("!failed to join stdin_command_reader thread");
        }
    }

    // join runner thread
    if (this->runner != NULL && this->runner > 0)
    {
        if (pthread_join(this->runner, 0) == 0) {
            std::cout << "joined runner thread.." << std::endl;
        } else {
            perror("!failed to join runner thread");
        }
    }

    if (this->sd >= 0) {
        int sd_old = this->sd;

        int try_close = 10;
        int res_close = -1;
        while (((res_close = close(this->sd)) < 0) && (try_close >= 0)) {
            perror("!socket cannot be closed jet");
            usleep(10000);
            try_close--;
        }

        if ((try_close < 0) && (res_close < 0)) {
            std::cout << "!could not close socket" << std::endl;
        } else {
            std::cout << "closed server socket: " << sd_old << std::endl;
        }
    }

    std::cout << "num open conns: " << this->connections.size() << std::endl;
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
    std::cout << "init listen to " << num_conns << " conns" << std::endl;
    assert (this->sd >= 0);

    if (listen(this->sd, num_conns) < 0) {
        perror("listen");
    }

    this->listen_active = true;
}

void
TCPServer::run(void *params) {
    assert(params != NULL);
    std::cout << "server mode: " << this->server_mode << std::endl;

    if ((this->server_mode == STOPPING) || (this->server_mode == STARTING)) {
        Cmd *cmd = (Cmd *) params;

        int num_conns = -1;
        if (cmd != NULL) {
            std::vector<std::string> params = cmd->params;
            std::vector<std::string>::iterator it;
            it = params.begin();

            if (it != params.end()) {
                num_conns = atoi((*it).c_str());
            }
        }

        if (num_conns < 0) {
            std::cout << std::endl;

            std::cout << "wrong params for running server.." << std::endl;

            // signal user that server is ready to read cmd
            std::cout << "[server] > ";
            std::cout.flush();

            return;
        }

        std::cout << std::endl;
        // create thread for joining listener threads (sperate thread for every connection)
        if (pthread_create(&this->collector, 0, collect, (void *)this) == 0) {
            std::cout << "collector thread created.." << std::endl;
        } else {
            perror("failed to create collector thread");
        }

        init_listen(num_conns);

        // signal user that server is ready to read cmd
        std::cout << "[server] > ";
        std::cout.flush();

        this->server_mode = RUNNING;
    } else {
        std::cout << std::endl;

        std::cout << "server allready running.." << std::endl;

        // signal user that server is ready to read cmd
        std::cout << "[server] > ";
        std::cout.flush();
    }
}

void
TCPServer::set_exit_extern(Sema *extern_exit) {
    this->exit_extern = extern_exit;
}

void
TCPServer::stop(void *params) {
    std::cout << "server mode: " << this->server_mode;
    if (this->server_mode == RUNNING) {
        std::cout << std::endl;
        std::cout << "stopping server.." << std::endl;
        this->server_mode = STOPPING;
        std::cout << "server mode is now.." << this->server_mode << std::endl;

        // necessary so blocked listener can move in to join queue
        this->select_client.V();

        // this->close_conns();

        // no open connections after server status set to STOPPING
        // wake up colletor thread to join
        if (this->connections.size() == 0) {
            this->threads_to_join.V();
        }

        // join colector thread
        std::cout << "start joining collector.." << std::endl;
        if (this->collector != NULL && this->collector > 0)
        {
            if (pthread_join(this->collector, 0) == 0) {
                std::cout << "joined collector.." << std::endl;
            } else {
                perror("!failed to join collector thread");
            }
        }

        std::cout << "stopped tcp server" << std::endl;

        // signal user that server is ready to read cmd
        std::cout << "[server] > ";
        std::cout.flush();
    } else {
        std::cout << std::endl;

        std::cout << "server allready stopped.." << std::endl;

        // signal user that server is ready to read cmd
        std::cout << "[server] > ";
        std::cout.flush();
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

            std::cout << "my server mode is: " << server->server_mode << std::endl;
            std::cout << "accept.." << std::endl;
            // conn = accept4(server->sd, (sockaddr*)&addr, &socklen, SOCK_NONBLOCK);
            conn = accept(server->sd, (sockaddr*)&addr, &socklen);

            if (conn < 0) {
                perror("accept");
            } else if (server->server_mode == RUNNING) {
                // set non-blocking
                // recv()/send() non blocking
                // set_nonblock(conn);

                server->connections[conn] = NULL;
                std::cout << std::endl;

                std::pair<TCPServer*, int> *listenerContext = new std::pair<TCPServer*, int>();
                listenerContext->first = server;
                listenerContext->second = conn;

                if(pthread_create(&server->connections[conn], 0, listener, (void *)listenerContext) == 0)
                {
                    std::cout << "create client thread for con .." << conn << std::endl;
                } else {
                    perror("Failed to create thread");
                }

                // send connection established to new client..
                std::cout << "send connection established.." << std::endl;
                std::string data = SERVER_CON_ESTABLISHED;
                if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                    perror("send failed!\n");
                }
                std::cout << "finished sending connection established.." << std::endl;

                // signal user that server is ready to read cmd
                std::cout << "[server] > ";
                std::cout.flush();
            }
        } else if (FD_ISSET(STDIN_FILENO, &server->read_flags)) {
            // std::cout << "you typed: " << sfd << std::endl;
            // start thread for working with input data
            std::string line;
            std::getline(std::cin, line);

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

    std::cout << std::endl;
    std::cout << "listener attached to conn " << conn << std::endl;

    // signal user that server is ready to read cmd
    std::cout << "[server] > ";
    std::cout.flush();

    while(1) {
        context->first->select_client.P();

        if (context->first->server_mode == STOPPING) {
            // free sema so other listener can join, too..
            context->first->select_client.V();

            // send BYE
            std::cout << std::endl;
            std::cout << "send bye" << std::endl;
            std::string data = SERVER_BYE;
            if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                perror("send failed!\n");
            }
            std::cout << "finished sending bye" << std::endl;

            // signal user that server is ready to read cmd
            std::cout << "[server] > ";
            std::cout.flush();

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
                // perror("data error");
            } else if (numRead == 0) {
                // client disconnect
                getpeername(conn , (struct sockaddr*)&context->first->addr , (socklen_t*)&context->first->addrlen);
                std::cout << std::endl;
                printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(context->first->addr.sin_addr) , ntohs(context->first->addr.sin_port));

                context->first->register_join_reqeust(conn);
                context->first->threads_to_join.V();

                delete context;

                // signal user that server is ready to read cmd
                std::cout << "[server] > ";
                std::cout.flush();

                break;
            } else {
                std::string msg = read_buffer;
                std::cout << std::endl;
                std::cout << "received: " << std::endl;
                std::cout << msg << std::endl;

                // send ack
                std::cout << "sending ack.." << std::endl;
                std::string data = SERVER_RECEIVED;
                if (send(conn, data.c_str(), strlen(data.c_str()), 0) < 0) {
                    perror("send ack failed!\n");
                }
                std::cout << "finished sending ack.." << std::endl;

                // signal user that server is ready to read cmd
                std::cout << "[server] > ";
                std::cout.flush();
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
    std::cout << "register join request for thread with conn " << conn << std::endl;
}

Cmd *
TCPServer::next_cmd_to_execute() {
    if (this->cmds_requested.empty()) {
        return new Cmd(CMD_UNAVAIL);
    }

    Cmd *cmd = this->cmds_requested.front();
    this->cmds_requested.pop();

    return cmd;
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
