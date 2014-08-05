#include "main.hpp"
#include "TCPClient.hpp"

#include <string>
#include <cstdlib>
#include <unistd.h>

#define WAIT_TIME 5
#define NUM_TEST_MSGS 4

int main() {
    std::cout << "****************************************" << std::endl;
    std::cout << "************** TCP CLIENT **************" << std::endl;
    std::cout << "****************************************" << std::endl;

    TCPClient *client = new TCPClient("127.0.0.1", 7186);
    //TCPClient *client = new TCPClient("x02.informatik.uni-bremen.de", 7160);
    //TCPClient *client = new TCPClient("www.heise.de", 80);
    //client->send_data("Test\n");

    for (int i=0; i<NUM_TEST_MSGS; i++) {
        if (client->get_sock() >= 0) {
            std::cout << "send_data started.. " << std::endl;
            client->send_data("GET / HTTP/1.0\r\nUser-Agent: Mozilla/5.0\r\nVary: Accept-Encoding,User-Agent\r\n\r\n");
            std::cout << "send_data ready.. " << std::endl;
        } else {
            std::cout << "abort send data.. " << std::endl;
            break;
        }

        sleep(WAIT_TIME);
    }

    return EXIT_SUCCESS;
}