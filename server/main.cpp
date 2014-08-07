#include "main.hpp"
#include "TCPServer.hpp"
extern "C"
{
#include "../common/assync_read.h"
}

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <string>
#include <sstream>

#define NUM_CONNS 10
#define RUN_TIME 30

int main() {
    std::cout << "****************************************" << std::endl;
    std::cout << "************** TCP SERVER **************" << std::endl;
    std::cout << "****************************************" << std::endl;
    
    TCPServer *server = new TCPServer("localhost", 7198);
    
    std::ostringstream ss;
    ss << NUM_CONNS;
    std::string conns = ss.str();
    std::cout << "parsed num cons to " << conns << std::endl;
    std::cout << std::endl;
    
    std::vector<std::string> params;
    params.push_back(conns);
    server->run(new Cmd(CMD_RUN, params));

    sleep(RUN_TIME);

    server->stop((void *) 0);

    server->~TCPServer();
    
    // while (true) sleep(1000);
    
    return EXIT_SUCCESS;
}