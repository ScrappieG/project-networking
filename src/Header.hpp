#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
#include<sys/socket.h>

struct handshake_header {
	std::array<char,18> header;
	std::array<char, 10> zero_bit = {0};
	uint32_t peer_id;

	handshake_header(uint32_t id) : peer_id(id){
		std::memcpy(header.data(), "P2PFILESHARINGPROJ", 18);
		zero_bit.fill(0);
	}
};

struct message {
	uint32_t length;//should not include the message length field itself
	uint8_t type;
	std::vector<char> payload; //may switch from string (unsure)
};

enum : uint8_t {
	CHOKE = 0x00,
	UNCHOKE = 0x01,
	INTERESTED = 0x02,
	UNINTERESTED = 0x03,
	REQUEST = 0x04,
	PIECE = 0x05,
	HAVE = 0x06,
	BITFIELD = 0x07,
};

//helper function for reading from recv
static bool read_exact(int sock, void* buf, size_t size){
	char* m_buf = static_cast<char*>(buf);
	size_t chars_left = size;

	while (chars_left > 0){
		ssize_t r = recv(sock, m_buf, chars_left, 0);
		if (r == 0) {
			return false; //peers connection closed
		}
		if (r < 0){
			if (errno == EINTR){ //connection interrupted
				continue;
			}
			return false;
		}
		m_buf += r; //move pointer forward
		chars_left -= static_cast<size_t>(r);
	}
	return true;
}

static bool send_exact(int sock, const void* buf, size_t size){
	const char* m_buf = static_cast<const char*>(buf);
	size_t chars_left = size;

	while (chars_left > 0){
		ssize_t s = send(sock, m_buf, chars_left, 0);
		if (s <= 0){
			if (s < 0 && errno == EINTR){
				continue;
			}
			return false;
		}
		m_buf += s;
		chars_left -= static_cast<size_t>(s);
	}
	return true;
}

static int ceiling_divide(unsigned int a, unsigned int b){
	if (a == 0){
		return 0;
	}
	return static_cast<int>((a + b - 1) /b);
}
