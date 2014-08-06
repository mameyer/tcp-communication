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
    UNKNOWN,
    PRINT_CONNS,
    HELP,
    NUM_IDS
};

class TCPServer
{
private:
    pthread_t collector;
    pthread_t runner;
    pthread_t stdin_command_reader;
    
    std::queue<int> join_requested;
    std::map<std::string, CmdIds> available_commands;
public:
    Sema threads_to_join;
    Sema access_connections;
    Sema select_client;
    Sema cmds_to_execute;
    bool connections_to_userspace;
    
    std::queue<CmdIds> cmds_requested;
    
    void (*cmdHandlers[NUM_IDS])(void *params);

    ServerMode server_mode;
    int sd;
    struct sockaddr_in addr;
    int addrlen;
    bool listen_active;
    std::map<int, pthread_t> connections;
    fd_set read_flags,write_flags;
    
    TCPServer(std::string addr, int port);
    ~TCPServer();
    
    void run(int num_conns);
    void init_listen(int num_conns);
    void stop();
    bool erase_conn(int conn);
    void close_conns();
    
    std::map<int, pthread_t> get_conns();
    int get_num_open_conns();
    pthread_t * find_thread(int conn);
    int next_to_join();
    int get_sd();
    CmdIds next_cmd_to_execute();
    
    void register_join_reqeust(int conn);
    void parse_cmd(std::string cmd);
    void register_cmd_handler(enum CmdIds id, void (*handler)(void *params));
    
    static void cmd_print_connections(void *params);
    static void cmd_show_help(void *params);
};

TCPServer tcpServer();

#endif