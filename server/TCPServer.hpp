#ifndef TCPSERVER_HPP__
#define TCPSERVER_HPP__

#include "../common/Sema.hh"

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <sys/select.h>
#include <netinet/in.h>

#define SERVER_CON_ESTABLISHED "SYN_ACK"
#define SERVER_RECEIVED "RCV_ACK"
#define SERVER_BYE "RCV_BYE"

void *collect(void * v);
void *listener(void *v);
void *accept_conn(void * v);
void *execute_cmd(void * v);

enum ServerMode {
    STARTING,   // create socket, bind socket, create thread for reading stdin input
    RUNNING,    // before this state: create thread for collecting listener threads,
    // accept and handle conns (creating runner thread) -> then: set state to RUNNING
    STOPPING,   // initiate stopping runner and collector thread
    CLOSING     //
};

enum CmdIds {
    CMD_UNAVAIL,
    CMD_UNKNOWN,
    CMD_PRINT_CONNS,
    CMD_HELP,
    CMD_STOP,
    CMD_RUN,
    CMD_EXIT,
    CMD_PRINT_INCOME,
    CMD_CLR_INCOME,
    CMD_NUM_IDS
};

class Cmd {
public:
    CmdIds id;
    std::vector<std::string> params;
    Cmd(CmdIds id, std::vector<std::string> params) {
        this->id=id;
        this->params=params;
    }
    
    Cmd(CmdIds id) {
        this->id=id;
    }
};

class TCPServer
{
private:
    pthread_t collector;
    pthread_t runner;
    pthread_t stdin_command_reader;
    
    Sema *exit_extern;

    std::queue<int> join_requested;
    std::map<std::string, CmdIds> available_commands;
public:
    Sema threads_to_join;
    Sema access_connections;
    Sema select_client;
    Sema cmds_to_execute;
    Sema access_income;
    Sema access_flush_stdin;
    bool connections_to_userspace;
    
    std::map<int, std::vector<std::string>*> income;

    std::queue<Cmd *> cmds_requested;

    void (TCPServer::*cmdHandlers[CMD_NUM_IDS])(void *params);

    ServerMode server_mode;
    int sd;
    struct sockaddr_in addr;
    int addrlen;
    bool listen_active;
    std::map<int, pthread_t> connections;
    fd_set read_flags,write_flags;

    TCPServer(std::string addr, int port);
    ~TCPServer();

    void run(void *params);
    void init_listen(int num_conns);
    void stop(void *params);
    bool erase_conn(int conn);
    void close_conns();

    std::map<int, pthread_t> get_conns();
    int get_num_open_conns();
    pthread_t * find_thread(int conn);
    int next_to_join();
    int get_sd();
    Cmd * next_cmd_to_execute();

    void register_join_reqeust(int conn);
    void parse_cmd(std::string cmd);

    void cmd_print_connections(void *params);
    void cmd_show_help(void *params);
    void cmd_exit(void *params);
    void print_income(void *params);
    void clr_income(void *params);
    
    void set_exit_extern(Sema *extern_exit);
    void flush_stdin();
};

#endif
