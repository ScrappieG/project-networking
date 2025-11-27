#include "Peer.hpp"
#include "Header.hpp"
#include "Neighbor.hpp"
#include "config.h"
#include <cstddef>
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
		logger_->line("Peer " + std::to_string(my_peer_id_) + " failed to create listening socket.");
		perror("failed while creating socket");
		return -1;
	}
	
	int opt = 1;
	int set_s = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (set_s < 0){
		logger_->line("Peer " + std::to_string(my_peer_id_) + " failed to set socket options.");
		debug_message("failed setting socket");
	}
		
	sockaddr_in addr{};
	addr.sin_family = AF_INET; //IPv4
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port_);
	
	if (::bind(s, (sockaddr*)&addr, sizeof(addr)) < 0){
		debug_message("Failed to bind");
		logger_->line("Peer " + std::to_string(my_peer_id_) + " failed to bind listening socket to port " + std::to_string(port_) + ".");
		close(s);
		return -1;
	}

	debug_message("bind() succeeded on socket " + std::to_string(s));

	if (listen(s, 128) < 0){
		debug_message("Failed to listen");
		logger_->line("Peer " + std::to_string(my_peer_id_) + " failed to listen on socket.");
		close(s);
		return -1;
	}

	logger_->line("Peer " + std::to_string(my_peer_id_) + " is listening for incoming connections on port " + std::to_string(port_) + ".");
	debug_message("DEBUG listen_on(): Socket " + std::to_string(s) + " is now LISTENING on port " + std::to_string(port_));

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
	logger_->line("Peer " + std::to_string(my_peer_id_) + " has stopped listening for incoming connections.");
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
		logger_->line("Peer " + std::to_string(my_peer_id_) + " failed to connect to " + peer_ip + ":" + std::to_string(peer_port) + ".");
	}
	logger_->line("Peer " + std::to_string(my_peer_id_) + " connected to " + peer_ip + ":" + std::to_string(peer_port) + ".");
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

	logger_->line("Peer " + std::to_string(my_peer_id_) + " sent handshake to Peer " + std::to_string(peer_id) + ".");
	return send_exact(sock, buf, sizeof(buf));
}

bool P2P_Client::read_handshake(int sock, std::string ip, uint16_t other_port, uint32_t expected_peer_id, bool has_file){
	char start_buf[18];
	if (!read_exact(sock, start_buf, 18)){
		return false;
	}
	
	const char f[19] = "P2PFILESHARINGPROJ";
	
	if (std::memcmp(start_buf, f, 18) != 0){
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

	//return on_new_connection(sock, ip, other_port, expected_peer_id, has_file);
	return true;
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

	for (char c : zero_buf){
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

	//return on_new_connection(sock, ip, other_port, peer_id, init_has_file);
	return true;
	
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

	logger_->line("Peer " + std::to_string(my_peer_id_) + " connected to Peer " + std::to_string(peer_id) + ".");
	start_peer_message_loop(sock);
	return true;
	
}

//accepting incoming connections
void P2P_Client::accept_loop(){
	logger_->line("Peer " + std::to_string(my_peer_id_) + " started accepting incoming connections.");
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
			debug_message("inet_ntop");
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

		uint32_t remote_peer_id = sock_to_peer_[cfd];
		bool has_file = false;
		on_new_connection(cfd, ip, other_port, remote_peer_id, has_file);
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

	on_new_connection(conn, ip, other_port, peer_id, has_file);

	return true;

}

bool P2P_Client::read_choke(int sock){
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return false;
	}
	n->set_choked(true);
	logger_->line("Peer " + std::to_string(my_peer_id_) 
		+ " received the 'choke' message from peer " 
		+ std::to_string(n->peer_id()) + ".");
	
	{
        std::lock_guard<std::mutex> lock(peers_mu_);
        auto it = piece_to_peer_.begin();
        while (it != piece_to_peer_.end()) {
            if (it->second == n->peer_id()) {
                requested_pieces_.erase(it->first);
                it = piece_to_peer_.erase(it);
            } else {
                ++it;
            }
        }
    }
	return true;
}

bool P2P_Client::read_unchoke(int sock){
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return false;
	}
	n->set_choked(false);

	logger_->line("Peer " + std::to_string(my_peer_id_) 
		+ " received the 'unchoke' message from peer " 
		+ std::to_string(n->peer_id()) + ".");
	
	request_next_piece(sock);

	return true;
}

bool P2P_Client::read_interested(int sock){
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return false;
	}
	n->set_interested(true);
	logger_->line("Peer " + std::to_string(my_peer_id_) + " received the 'interested' message from peer " + std::to_string(n->peer_id()) + ".");

	return true;
}

bool P2P_Client::read_uninterested(int sock){
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return false;
	}

	n->set_interested(false);

	logger_->line("Peer " + std::to_string(my_peer_id_) 
		+ " received the 'not interested' message from peer " 
		+ std::to_string(n->peer_id()) + ".");
	
	return true;
}

bool P2P_Client::read_have(int sock, std::vector<char> buf){
	if (buf.size() < 4){
		return false;
	}

	u_int32_t piece_index_net = 0;
	std::memcpy(&piece_index_net, buf.data(), 4);
	int piece_index = static_cast<int>(ntohl(piece_index_net));

	if ( piece_index < 0 || piece_index >= total_pieces_){
		return false;
	}
	Neighbor* n = find_neighbor_by_sock(sock);

	if (n == nullptr){
		return false;
	}
	n->set_piece(piece_index, true);
	logger_->line("Peer " + std::to_string(my_peer_id_) 
		+ " received the 'have' message from peer " 
		+ std::to_string(n->peer_id()) 
		+ " for piece " + std::to_string(piece_index) + ".");
	
	bool need_piece = !has_piece(piece_index);
	bool already_interested = n->interested();

	if (need_piece && !already_interested){
		if (!send_message(INTERESTED, nullptr, 0, sock)){
			return false;
		}
		n->set_interested(true);
		logger_->line("Peer " + std::to_string(my_peer_id_) 
			+ " sent the 'interested' message to peer " 
			+ std::to_string(n->peer_id()) + ".");
	}
	return true;
}

bool P2P_Client::read_request(int sock, std::vector<char> buf){
	if (buf.size() < 4){
		return false;
	}

	uint32_t piece_index_net = 0;
	std::memcpy(&piece_index_net, buf.data(), 4);
	int piece_index = static_cast<int>(ntohl(piece_index_net));
	if ( piece_index < 0 || piece_index >= total_pieces_){
		return false;
	}
	
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return false;
	}
	
	std::vector<char> piece_data;
    if (!read_piece_from_file(piece_index, piece_data)) {
		logger_->event("ERROR", "Failed to read piece " + std::to_string(piece_index) + " from file for peer " + std::to_string(n->peer_id()) + ".");
		debug_message("Failed to read piece " + std::to_string(piece_index) + " from file");
        return false;
    }

	if (n->choked()){
		debug_message("Peer " + std::to_string(my_peer_id_) + " received a request for piece " + std::to_string(piece_index) 
			+ " from peer " + std::to_string(n->peer_id()) + " but is currently choked.");
		
		return true;
	}

	std::vector<char> payload(4 + piece_data.size());
	uint32_t piece_net = htonl(static_cast<uint32_t>(piece_index));
	std::memcpy(payload.data(), &piece_net, 4);
    std::memcpy(payload.data() + 4, piece_data.data(), piece_data.size());

	if (!send_message(PIECE, payload.data(), payload.size(), sock)) {
		logger_->event("ERROR", "Failed to send piece " + std::to_string(piece_index) + " to peer " + std::to_string(n->peer_id()) + ".");
		debug_message("Failed to send piece " + std::to_string(piece_index) + " to peer " + std::to_string(n->peer_id()));

        return false;
    }

	return true;
}

bool P2P_Client::read_piece(int sock, std::vector<char> buf){
	if (buf.size() < 4){
		return false;
	}

	uint32_t piece_index_net = 0;
	std::memcpy(&piece_index_net, buf.data(), 4);
	int piece_index = static_cast<int>(ntohl(piece_index_net));

	if (piece_index >= static_cast<uint32_t>(total_pieces_)){
		return false;
	}
	else if (piece_index < 0){
		return false;
	}

	std::vector<char> piece_data(buf.begin() + 4, buf.end());

	if (has_piece(piece_index)){
		debug_message("Received piece we already have: " + std::to_string(piece_index));
		logger_->event("WARNING", "Received piece we already have: " + std::to_string(piece_index));

		return true;
	}
	if (!write_piece_to_file(piece_index, piece_data)){
		debug_message("Failed to write piece to file: " + std::to_string(piece_index));
		logger_->event("ERROR", "Failed to write piece to file: " + std::to_string(piece_index));

		return false;
	}
	set_bitfield_bit(piece_index, true);

	//send HAVE message to all neighbors
	uint32_t have_index_net = htonl(piece_index);
	{
		std::lock_guard<std::mutex> lck(peers_mu_);
		for (auto* n : neighbors_){
			if (!send_message(HAVE, &have_index_net, sizeof(have_index_net), n->sock())){
				debug_message("Failed to send HAVE message to peer: " + std::to_string(n->peer_id()));
				logger_->event("ERROR", "Failed to send HAVE message to peer: " + std::to_string(n->peer_id()));
			}
		}
	}

	int pieces_have = 0;
	for (size_t i = 0; i < total_pieces_; ++i){
		if (has_piece(i)){
			pieces_have++;
		}
	}
	Neighbor* neighbor = find_neighbor_by_sock(sock);
	if (neighbor) {
		logger_->line("Peer " + std::to_string(my_peer_id_) 
			+ " has downloaded piece " + std::to_string(piece_index) 
			+ " from peer " + std::to_string(neighbor->peer_id())
			+ ". Now has " + std::to_string(pieces_have) 
			+ " pieces.");
	}

	if (has_complete_file()){
		debug_message("Peer " + std::to_string(my_peer_id_) + " has downloaded the complete file.");
		logger_->event("INFO", "Peer " + std::to_string(my_peer_id_) + " has downloaded the complete file.");
	} else {
		debug_message("Peer " + std::to_string(my_peer_id_) + " has not yet downloaded the complete file.");
		logger_->event("INFO", "Peer " + std::to_string(my_peer_id_) + " has not yet downloaded the complete file.");

		Neighbor* n = find_neighbor_by_sock(sock);
		if (n && !n->choked()){
			
			{
            std::lock_guard<std::mutex> lock(peers_mu_);
            requested_pieces_.erase(piece_index);
            piece_to_peer_.erase(piece_index);
        	}
			request_next_piece(sock);
		}
	}

	return true;
}

bool P2P_Client::read_bitfield(int sock, std::vector<char> buf){
	//updates the hasFile of the neighbor
	set_hasFile_from_bf(sock, buf);
	
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return false;
	}

	n->init_bitfield(total_pieces_);

	for (size_t i = 0; i < buf.size() && i < n->get_bitfield().size(); i++) {
        n->get_bitfield()[i] = static_cast<uint8_t>(buf[i]);
    }

	bool have_interesting_pieces = false;
    for (int i = 0; i < total_pieces_; i++) {
        if (!has_piece(i) && n->has_piece(i)) {
            have_interesting_pieces = true;
            break;
        }
    }

	if (have_interesting_pieces) {
        send_message(INTERESTED, nullptr, 0, sock);
        n->set_interested(true);
    } else {
        send_message(UNINTERESTED, nullptr, 0, sock);
        n->set_interested(false);
    }

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

//reads bitfield from disk into memory
bool P2P_Client::read_piece_from_file(int piece_index, std::vector<char>& piece_data){
	std::lock_guard<std::mutex> lck(file_mu_);

	size_t offset = static_cast<size_t>(piece_index) * piece_size_;

	size_t this_piece_size = piece_size_;
	if (piece_index == total_pieces_ - 1){
		this_piece_size = file_size_ - (piece_index * piece_size_);
	}

	std::string file_path = "peer_" + std::to_string(my_peer_id_) + "/" + file_name_;
    std::ifstream file(file_path, std::ios::binary);

	if (!file){
		return false;
	}

	file.seekg(offset, std::ios::beg);
	if (!file){
		return false;
	}
	piece_data.resize(this_piece_size);
	file.read(piece_data.data(), this_piece_size);
	if (!file){
		return false;
	}
	return true;
}

	
bool P2P_Client::write_piece_to_file(int piece_index, std::vector<char>& piece_data){
	std::lock_guard<std::mutex> lck(file_mu_);

	size_t offset = static_cast<size_t>(piece_index) * piece_size_;
	size_t this_piece_size = piece_size_;
	if (piece_index == total_pieces_ - 1){
		this_piece_size = file_size_ - (piece_index * piece_size_);
	}

	if (piece_data.size() != this_piece_size){
		return false;
	}

	std::string file_path = "peer_" + std::to_string(my_peer_id_) + "/" + file_name_;
    std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out);
	
	//if file does not exist create it
	if (!file){
		file.open(file_path, std::ios::binary | std::ios::out);
		if (!file){
			return false;
		}
		file.close();

		//reopen in read/write mode
		file.open(file_path,  std::ios::binary | std::ios::in | std::ios::out);
		if (!file){
			return false;
		}
	}

	file.seekp(offset, std::ios::beg);
	if (!file){
		return false;
	}

	file.write(piece_data.data(), this_piece_size);
	file.flush();

	if (!file){
		return false;
	}
	return true;
}

bool P2P_Client::has_piece_on_disk(int piece_index) const {
	debug_message("has piece on disk(" + std::to_string(piece_index) + ") called");
	std::string piece_file = Config::peerDirName(my_peer_id_) + "/" + file_name_;
	debug_message("Checking file: " + piece_file);

	std::lock_guard<std::mutex> lck(file_mu_);

	size_t offset = static_cast<size_t>(piece_index) * piece_size_;
	size_t this_piece_size = piece_size_;
	if (piece_index == total_pieces_ - 1){
		this_piece_size = file_size_ - (piece_index * piece_size_);
	}

	std::string file_path = "peer_" + std::to_string(my_peer_id_) + "/" + file_name_;
	std::ifstream file(file_path, std::ios::binary);

	if (!file){
		return false;
	}

	file.seekg(0, std::ios::end);
	size_t file_size = static_cast<size_t>(file.tellg());

	if (file_size < offset + this_piece_size){
		return false;
	}
	
	return true;
}

void P2P_Client::set_bitfield_bit(int piece_index, bool value){
	size_t byte = piece_index / 8;
	size_t bit = 7 - (piece_index % 8);

	if (byte >= bitfield_.size()){
		return;
	}

	if (value){
		bitfield_[byte] |= (1 << bit);
	} else {
		bitfield_[byte] &= ~(1 << bit);
	}
}

bool P2P_Client::has_piece(int piece_index) const {
	size_t byte = piece_index / 8;
	size_t bit = 7 - (piece_index % 8);

	if (byte >= bitfield_.size()){
		return false;
	}

	unsigned char b = static_cast<unsigned char>(bitfield_[byte]);
	return ((b >> bit) & 1) != 0;
}

bool P2P_Client::has_complete_file() const{
	for (size_t i = 0; i < total_pieces_; ++i){
		if (!has_piece(i)){
			return false;
		}
	}
	return true;
}

void P2P_Client::peer_message_loop(int sock){
	while (true){
		if (!read_message(sock)){
			debug_message("Failed to read message from peer socket: " + std::to_string(sock));
			logger_->event("ERROR", "Failed to read message from peer socket: " + std::to_string(sock));
			{
				std::lock_guard<std::mutex> lck(peers_mu_);
				auto it = sock_to_peer_.find(sock);
				if (it != sock_to_peer_.end()){
					logger_->event("DISCONNECT", "Lost connection to peer " + std::to_string(it->second));
					sock_to_peer_.erase(it);

				}
			}
			break;
		}
	}
}

void P2P_Client::start_peer_message_loop(int sock){
	peer_threads_.push_back(std::thread(&P2P_Client::peer_message_loop, this, sock));
}

void P2P_Client::request_next_piece(int sock){
	Neighbor* n = find_neighbor_by_sock(sock);
	if (n == nullptr){
		return;
	}

	int piece_to_request = -1;
	for (int i = 0; i < total_pieces_; ++i){
		if (!has_piece(i) && n->has_piece(i)){
			piece_to_request = i;
			break;
		}
	}

	if (piece_to_request == -1){
		if (n->interested()) {
            send_message(UNINTERESTED, nullptr, 0, sock);
        }

		return;
	}

	{
        std::lock_guard<std::mutex> lock(peers_mu_);
        requested_pieces_.insert(piece_to_request);
        piece_to_peer_[piece_to_request] = n->peer_id();
    }

	uint32_t piece_net = htonl(static_cast<uint32_t>(piece_to_request));
    send_message(REQUEST, &piece_net, sizeof(piece_net), sock);
}

void P2P_Client::unchoke_timer_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(unchoking_interval_));
        select_preferred_neighbors();
    }
}

void P2P_Client::select_preferred_neighbors() {
	std::lock_guard<std::mutex> lock(peers_mu_);

	std::vector<Neighbor*> interested_neighbors;
	for (auto* n : neighbors_) {
		if (n->interested()) {
			interested_neighbors.push_back(n);
		}
	}

	std::set<uint32_t> current_preferred;

	for (size_t i = 0; i < num_pref_neighbors_ && i < interested_neighbors.size(); ++i) {
		current_preferred.insert(interested_neighbors[i]->peer_id());

		if (interested_neighbors[i]->choked()) {
			send_message(UNCHOKE, nullptr, 0, interested_neighbors[i]->sock());
			interested_neighbors[i]->set_choked(false);
		}

	}

	for (auto* n : neighbors_) {
		if (current_preferred.find(n->peer_id()) == current_preferred.end()) {
			if (!n->choked()) {
				send_message(CHOKE, nullptr, 0, n->sock());
				n->set_choked(true);
			}
		}

	}

	preferred_neighbors_ = std::move(current_preferred);
	std::string neighbor_list;
	for (auto id : preferred_neighbors_) {
		if (!neighbor_list.empty()) neighbor_list += ",";
		neighbor_list += std::to_string(id);
	}
	logger_->line("Peer " + std::to_string(my_peer_id_) + " has the preferred neighbors " + neighbor_list + ".");

}