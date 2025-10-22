#include "Peer.hpp"
#include "Header.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <netdb.h>
#include <vector>

Neighbor* P2P_Client::find_neighbor_by_id(uint32_t id){
	for (auto* n : neighbors_){
      		if (n->peer_id() == id){
			return n;
      		}
	}
      	return nullptr;
}

Neighbor* P2P_Client::find_neighbor_by_sock(int sock){
	auto it = sock_to_peer_.find(sock);
	if (it == sock_to_peer_.end()){
		return nullptr;
	}
	return find_neighbor_by_id(it->second);
}
      
int P2P_Client::listen_on(){
	//Uses TCP, IPv4 (unsure if it should be IPv4)
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("failed while creating socket");
		return -1;
	}
	
	int opt = 1;
	int set_s = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (set_s < 0){
		perror("failed setting socket");
		//maybe return idk
	}
		
	sockaddr_in addr{};
	addr.sin_family = AF_INET; //IPv4
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port_);
	
	if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0){
		perror("Failed to bind");
		close(s);
		return -1;
	}

	if (listen(s, 128) < 0){
		perror("Failed to listen");
		close(s);
		return -1;
	}
	return s;

}

void P2P_Client::stop_listening() {
	bool expected = true;

	if (!accepting_.compare_exchange_strong(expected, false)){
		return; //thread is not running
	}

	if (listening_sock_ >= 0){
		shutdown(listening_sock_, SHUT_RD);
		close(listening_sock_);
		listening_sock_ = -1;
	}
	if (accept_thread_.joinable()){
		accept_thread_.join();
	}
}


int P2P_Client::connect_to(std::string peer_ip, uint16_t peer_port){
	
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	std::string port_str = std::to_string(peer_port);
	addrinfo* result = nullptr;
	int result_check = getaddrinfo(peer_ip.c_str(), port_str.c_str(), &hints, &result);
	if (result_check != 0){
		return -1;
	}

	int s = -1;
	for (addrinfo* it = result; it != nullptr; it = it->ai_next){
		s = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (s < 0){
      			continue;
      		}
		if (connect(s, it->ai_addr, it->ai_addrlen) == 0){
      			break;
      		}
		close(s);
		s = -1;
	}
	freeaddrinfo(result);
	if (s < 0){
		perror("connect");
	}
	return s;
}


bool P2P_Client::send_message(uint8_t type, const void* payload, uint32_t payload_len, int sock){
	const uint32_t length = 1 + payload_len;
	if (length < 1) {
		return false;
	}
	uint32_t nlen = htonl(length);
	
	//send 4 byte length
	if (!send_exact(sock, &nlen, sizeof(nlen))){
		return false;
	}

	// send 1 byte type
	if (!send_exact(sock, &type, sizeof(type))){
		return false;
	}

	// send variable length payload (payload can be empty)
	if (payload_len > 0){
		if (payload == nullptr){
			return false;
		}
		if (!send_exact(sock, payload, payload_len)){
			return false;
		}
	}
	return true;
}

//convert a char buffer to a string
static std::string convert_to_string(const char* buf, size_t size){
	size_t i = size;

	while (i > 0 && buf[i-1] == '\0'){//solves a problem with multiple trailing null terminators
		i--;
	}
	return std::string(buf, i);
}

//read the actual messages from peer
bool P2P_Client::read_message(int sock){
	//read in 4 byte message length
	uint32_t t_len;
	if (!read_exact(sock, &t_len, sizeof(t_len))){
		return false;
	}
	uint32_t m_length = ntohl(t_len); //fix order
	
	if (m_length < 1){
		return false;
	}
	uint32_t payload_length = m_length - 1;
	

	//read in 1 byte message type
	uint8_t type = 0;	
	if (!read_exact(sock, &type, sizeof(type))){
		return false;
	}
		
	//read in payload if applicable
	std::vector<char> payload(payload_length);
	if (payload_length > 0){
		if (!read_exact(sock, payload.data(), payload.size())){
			return false;
		}
	}

	if (type == CHOKE){//choke
		return read_choke(sock);
	}
	else if (type == UNCHOKE){//unchoke
		return read_unchoke(sock);
	}
	else if (type == INTERESTED){//interested
		return read_interested(sock);
	}
	else if (type == UNINTERESTED){//uninterested
		return read_uninterested(sock);
	}
	else if (type == REQUEST){//request
		return read_request(sock, payload);
	}
	else if (type == PIECE){//piece
		return read_piece(sock, payload);	
	}
	else if (type == HAVE){//have
		return read_have(sock, payload);
	}
	else if (type == BITFIELD){//bitfield
		return read_bitfield(sock, payload);

	}
	return false;
	
	
}

int P2P_Client::start_listening() {
	if (listening_sock_ >= 0) {
		return listening_sock_;
	}

	listening_sock_ = listen_on();
	if (listening_sock_ < 0){
		return -1;
	}

	//this might be overkill and it may end up blowing up but im going to try to
	//use threads since I believe we want to be able to listen and send at the same time
	//this could become a problem but we'll see
	
	bool expected = false;
	if (!accepting_.compare_exchange_strong(expected, true)){
		return listening_sock_;
	}
	
	accept_thread_ = std::thread(&P2P_Client::accept_loop, this);
	return listening_sock_;

}

bool P2P_Client::send_handshake(int sock, uint32_t peer_id){
	const char f[19] = "P2PFILESHARINGPROJ";

	char buf[32];
	std::memcpy(buf, f, 18); //P2PFILESHARINGPROJ
	std::memset(buf+18, 0, 10); //zeros
	uint32_t net_peer_id = htonl(peer_id);
	std::memcpy(buf+28, &net_peer_id, 4);
	return send_exact(sock, buf, sizeof(buf));
}

bool P2P_Client::read_handshake(int sock, std::string ip, uint16_t other_port, uint32_t expected_peer_id, bool has_file){
	char start_buf[18];
	if (!read_exact(sock, start_buf, 18)){
		return false;
	}
	
	const char f[19] = "P2PFILESHARINGPROJ";
	
	if (std::memcmp(start_buf, f, 18) != 0){//does first 18 bits == expected (P2P...)
		return false;
	}
	
	char zero_buf[10];
	if(!read_exact(sock, zero_buf, sizeof(zero_buf))){
		return false;
	}

	for (char c: zero_buf){
		if (c != 0){
		      return false;
		}
	}

	char id_buf[4];
	if (!read_exact(sock, id_buf, sizeof(id_buf))){
		return false;
	}
	uint32_t net_peer_id = 0;
	std::memcpy(&net_peer_id, id_buf, 4);

	uint32_t peer_id = ntohl(net_peer_id);

	if (peer_id != expected_peer_id){
		return false;
	}

	{
		std::lock_guard<std::mutex> lck(peers_mu_);
		sock_to_peer_[sock] = peer_id;
	}

	return on_new_connection(sock, ip, other_port, expected_peer_id, has_file);
}


bool P2P_Client::read_handshake(int sock, std::string ip, uint16_t other_port){
	char start_buf[18];
	if (!read_exact(sock, start_buf, 18)){
		return false;
	}
	
	const char f[19] = "P2PFILESHARINGPROJ";
	
	if (std::memcmp(start_buf, f, 18) != 0){//does first 18 bits == expected (P2P...)
		return false;
	}
	
	char zero_buf[10];
	if(!read_exact(sock, zero_buf, sizeof(zero_buf))){
		return false;
	}

	for (char c: zero_buf){
		if (c != 0){
		      return false;
		}
	}

	char id_buf[4];
	if (!read_exact(sock, id_buf, sizeof(id_buf))){
		return false;
	}
	uint32_t net_peer_id = 0;
	std::memcpy(&net_peer_id, id_buf, 4);

	uint32_t peer_id = ntohl(net_peer_id);

	bool init_has_file = false;

	{
		std::lock_guard<std::mutex> lck(peers_mu_);
		auto it = neighbor_has_file.find(peer_id);
		if (it != neighbor_has_file.end()){
			init_has_file = it->second;
		}
		sock_to_peer_[sock] = peer_id;
	}

	return on_new_connection(sock, ip, other_port, peer_id, init_has_file);
	
}


void P2P_Client::addNeighbor(int sock, std::string ip, uint16_t port, uint32_t peer_id, bool has_file){
	std::lock_guard<std::mutex> l(peers_mu_); //lock the peers vector (THIS IS IMPORTANT FOR THREADING)
	neighbors_.push_back(new Neighbor(sock, port,ip, peer_id, has_file));
}

bool P2P_Client::on_new_connection(int sock, std::string ip, uint16_t port, uint32_t peer_id, bool has_file){
	addNeighbor(sock, ip, port, peer_id, has_file);	
	//once connnection is established send bitfield message
	
	uint32_t bitfield_len = bitfield_.size();
	if (!send_message(BITFIELD, bitfield_.data(), bitfield_len, sock)){
		return false;
	}
	return true;
	
}

//accepting incoming connections
void P2P_Client::accept_loop(){
	while(accepting_){
		
		sockaddr_in other_addr{};
		socklen_t s = sizeof(other_addr);
		int cfd = accept(listening_sock_, reinterpret_cast<sockaddr*>(&other_addr), &s);
		if (cfd < 0) {
			if (!accepting_){
				break;
			}
			if (errno == EINTR){
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK){
				continue;
			}
			continue;
		}

		char ip_str[INET_ADDRSTRLEN] = {};
		
		if(!inet_ntop(AF_INET, &other_addr.sin_addr, ip_str, sizeof(ip_str))){
			perror("inet_ntop");
			close(cfd);
			continue;
		}
		uint16_t other_port = ntohs(other_addr.sin_port);
		
		std::string ip = std::string(ip_str);

		if (!read_handshake(cfd, ip, other_port)){
			close(cfd);
			continue;
		}
		if (!send_handshake(cfd, my_peer_id_)){
			close(cfd);
		}
	
	}
}

//accept and handshake outgoing connection
bool P2P_Client::connect_and_handshake(std::string ip, uint16_t other_port, int peer_id, bool has_file){
	int conn = connect_to(ip, other_port);

	if (conn < 0){
		return false;
	}

	if (!send_handshake(conn, my_peer_id_)){
		close(conn);
		return false;
	}
	if (!read_handshake(conn, ip, other_port,peer_id, has_file)){
		close(conn);
		return false;
	}

	return true;

}

bool P2P_Client::read_choke(int sock){
	//TODO
	return true;
}

bool P2P_Client::read_unchoke(int sock){
	//TODO
	return true;
}

bool P2P_Client::read_interested(int sock){
	//TODO
	return true;
}

bool P2P_Client::read_uninterested(int sock){
	//TODO
	return true;
}

bool P2P_Client::read_have(int sock, std::vector<char> buf){
	//TODO
	return true;
}

bool P2P_Client::read_request(int sock, std::vector<char> buf){
	//TODO
	return true;
}

bool P2P_Client::read_piece(int sock, std::vector<char> buf){
	//TODO
	return true;
}

bool P2P_Client::read_bitfield(int sock, std::vector<char> buf){
	//updates the hasFile of the neighbor
	set_hasFile_from_bf(sock, buf);
	//TODO update the actual bitfield of the neighbor
	return true;
}

//sets whether or not the neighbor has the entire file (by looking if its bitmap is full of 1s)
bool P2P_Client::set_hasFile_from_bf(int sock, std::vector<char> buf){
	const size_t num_pieces = static_cast<size_t>(total_pieces_);

	if (buf.empty() || num_pieces == 0){
		return true;
	}

	bool entire_file = true;

	for (size_t i = 0; i < num_pieces; ++i){
		size_t byte = i/8;
		size_t bit = 7- (i % 8);

		if (byte >= buf.size()){
			entire_file = false;
			break;
		}

		unsigned char b = static_cast<unsigned char>(buf[byte]);
		bool bit_set = ((b >> bit) & 1) != 0;
		if (!bit_set){
			entire_file = false;
			break;
		}
	}

	std::lock_guard<std::mutex> lck(peers_mu_);
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n != nullptr){
		n->set_has_file(true);
		return true;
		}
	return false;
		

}

	
