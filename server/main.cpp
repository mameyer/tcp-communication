#include "main.hpp"
#include "TCPServer.hpp"
extern "C"
{
#include "../common/assync_read.h"
}

#include <cstdlib>
#include <iostream>
#include <unistd.h>

#define NUM_CONNS 10
#define RUN_TIME 30

int main() {
    std::cout << "****************************************" << std::endl;
    std::cout << "************** TCP SERVER **************" << std::endl;
    std::cout << "****************************************" << std::endl;

    TCPServer *server = new TCPServer("localhost", 7190);
    server->run(NUM_CONNS);
    std::cout << "server started.." << std::endl;
    std::cout << std::endl;

    sleep(RUN_TIME);

    server->stop();

    std::cout << std::endl;
    std::cout << "server stopped.." << std::endl;
    
    sleep(5);

    std::cout << std::endl;
    std::cout << "open connections:" << std::endl;

    std::map<int, pthread_t> conns = server->get_conns();
    if (conns.size() == 0) {
        std::cout << "no open connections!" << std::endl;
    } else {
        std::map<int, pthread_t>::iterator it;

        for (it = conns.begin(); it != conns.end(); it++) {
            std::cout << it->first << std::endl;
        }
    }
    
    // sleep(10);

    server->~TCPServer();
    return EXIT_SUCCESS;
}