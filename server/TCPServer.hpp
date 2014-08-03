#ifndef TCPSERVER_HPP__
#define TCPSERVER_HPP__

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
void *reader(void *v);
void *writer(void *v);

class TCPServer
{
private:
    pthread_t collector;
    pthread_t runner;
    std::queue<int> join_requested;
public:
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
    pthread_t * find_thread(int conn);
    int next_to_join();
    int get_sd();
    
    void set_nonblock(int socket);
    void register_join_reqeust(int conn);
};

TCPServer tcpServer();

#endif