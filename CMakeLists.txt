project(TCPCom)

set(COMPILE_DEFINITIONS_DEBUG    -O3 -ggdb)
set(COMPILE_DEFINITIONS_RELEASE  -O3)

add_definitions(
    -Wall -Wextra
    -O3 -ggdb
)

include_directories(
    server
    client
    common
)

add_executable(server
    common/assync_read.c
    server/main.cpp
    server/TCPServer.cpp
)
target_link_libraries(server pthread)

add_executable(client
    client/main.cpp
    client/TCPClient.cpp
)
target_link_libraries(client pthread)