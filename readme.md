# Testing

Testing for test_peer.cpp
    `g++ -std=c++20 -Wall -Wextra -Isrc src/peer.cpp tests/test_peer.cpp -o test_peer`
This is a pretty simple test and really only to test the handshake.
Note: this will scream at you since some things haven't been fully implemented
