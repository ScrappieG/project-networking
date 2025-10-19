#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <cstring>

/*
NOTES:
Using TCP
symmetrical interaction between peers (messages are sent in both directions at same time)

handshake parts:
1. handshake header (18 byte string) (P2PFILESHARINGPROJ)
2. zero bits (10 bytes zero bits)
3. peer ID (4-byte integer)


*/

struct handshake_header {
	std::array<char,18> header;
	std::array<char, 10> zero_bit = {0};
	uint32_t peer_id;


	//this is kind of a weird way to do it but I want to ensure that the vars are of correct length
	handshake_header(uint32_t id) : peer_id(id){
		std::memcpy(header.data(), "P2PFILESHARINGPROJ", 18);
		zero_bit.fill(0);
	}
};


/*
 * message type values -> message type
 * 0->choke
 * 1->unchoke
 * 2->interested
 * 3->not interested
 * 4->have
 * 5->bitfield
 * 6->request
 * 7->piece
*/
struct message {
	uint32_t length;//should not include the message length field itself
	uint8_t type;
	std::string payload; //may switch from string (unsure)
};


class P2P_Client {
public:
	P2P_Client(int port) : port(port) {}
private:
	unsigned int port;
	unsigned int preferred_neighbors;
	unsigned int unchoking_interval;
	std::string file_name;
	unsigned int filesize;
	unsigned int piece_size;

	int create_listen_socket();

};

