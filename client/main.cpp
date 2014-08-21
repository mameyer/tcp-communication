#include "main.hpp"
#include "TCPClient.hpp"

#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sstream>

#define WAIT_TIME 2
#define NUM_TEST_MSGS 4
#define NUM_TEST_CLIENTS 4

int main() {
    std::cout << "****************************************" << std::endl;
    std::cout << "************** TCP CLIENT **************" << std::endl;
    std::cout << "****************************************" << std::endl;

    TCPClient *clients[NUM_TEST_CLIENTS];
    for (int i=0; i<NUM_TEST_CLIENTS; i++) {
        clients[i] = new TCPClient("127.0.0.1", 7777);
    }

    //TCPClient *client = new TCPClient("x02.informatik.uni-bremen.de", 7160);
    //TCPClient *client = new TCPClient("www.heise.de", 80);
    //client->send_data("Test\n");

    for (int i=0; i<NUM_TEST_MSGS; i++) {
        for (int j=0; j<NUM_TEST_CLIENTS; j++) {
            if (clients[j] != NULL) {
                if (clients[j]->get_sock() >= 0) {
                    std::cout << "send_data started.. " << std::endl;
                    // clients[j]->send_data("GET / HTTP/1.0\r\nUser-Agent: Mozilla/5.0\r\nVary: Accept-Encoding,User-Agent\r\n\r\n");
                    std::ostringstream convert;
                    convert << "HELO FROM " << clients[j]->get_sock();
                    clients[j]->send_data(convert.str());
                    std::cout << "send_data ready.. " << std::endl;
                } else {
                    std::cout << "abort send data.. " << std::endl;
                    clients[j]->~TCPClient();
                    clients[j] = NULL;
                    break;
                }
            }
        }
        
        sleep(WAIT_TIME);
    }

    for (int i=0; i<NUM_TEST_CLIENTS; i++) {
        if (clients[i] != NULL) {
            clients[i]->~TCPClient();
        }
    }

    return EXIT_SUCCESS;
}
