#include "main.hpp"
#include "TCPServer.hpp"

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
    
    Sema exit(0);
    TCPServer *server = new TCPServer("localhost", 7198);
    server->set_exit_extern(&exit);
    
    /*std::ostringstream ss;
    ss << NUM_CONNS;
    std::string conns = ss.str();
    std::cout << "parsed num cons to " << conns << std::endl;
    std::cout << std::endl;
    
    std::vector<std::string> params;
    params.push_back(conns);
    server->run(new Cmd(CMD_RUN, params));

    sleep(RUN_TIME);

    server->stop((void *) 0);

    server->~TCPServer();*/
    
    exit.P();
    server->~TCPServer();
    
    return EXIT_SUCCESS;
}