
#include "../src/Peer.hpp"
#include "../src/Header.hpp"
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

int main() {
    unsigned int file_size  = 128 * 1024;  // 128 KiB
    unsigned int piece_size = 32 * 1024;   // 32 KiB

    // Note: ctor now takes a final bool has_file
    P2P_Client clientA(1001, 5001, "127.0.0.1", 0, 0, "temp.txt", file_size, piece_size, true);
    std::cout << "Created ClientA [OK]" << std::endl;

    P2P_Client clientB(1002, 5002, "127.0.0.1", 0, 0, "temp.txt", file_size, piece_size, false);
    std::cout << "Created ClientB [OK]" << std::endl;

    // socketpair for testing
    int sv[2];
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    // handshake test
    assert(clientA.send_handshake(sv[0], clientA.peer_id()));
    assert(clientB.read_handshake(sv[1], "127.0.0.1", clientA.port()));
    std::cout << "handshake test [OK]" << std::endl;

    // simple message: HAVE
    uint32_t piece = htonl(1);
    assert(clientA.send_message(HAVE, &piece, sizeof(piece), sv[0]));
    assert(clientB.read_message(sv[1])); // should route to read_have(...)
    std::cout << "simple message test [OK]" << std::endl;

    ::close(sv[0]);
    ::close(sv[1]);
    return 0;
}
