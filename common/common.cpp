#include "common.hpp"

#include <fcntl.h>
#include <assert.h>

// http://stackoverflow.com/questions/6715736/using-select-for-non-blocking-sockets
void set_nonblock(int socket)
{
    int flags;
    flags = fcntl(socket,F_GETFL,0);
    assert(flags != -1);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}