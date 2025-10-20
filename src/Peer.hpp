#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "Neighbor.hpp"
#include "Header.hpp"
#include <thread>
#include <atomic>

class P2P_Client {

private:
	uint16_t port_;
	int listening_sock_;
	unsigned int num_pref_neighbors_;
	unsigned int unchoking_interval_;
	std::string file_name_;
	unsigned int file_size_;
	unsigned int piece_size_;
	uint32_t my_peer_id_;
	std::string ip_;
	int total_pieces_;
	bool has_file_;

	std::vector<Neighbor*> neighbors_;
	std::vector<uint8_t> bitfield_;
	
	std::thread accept_thread_;
	std::atomic<bool> accepting_{false};
	std::mutex peers_mu_;
	
public:
	//Constructor
	P2P_Client(uint32_t peer_id, int port,std::string ip, unsigned int num_pref_neighbors, unsigned int unchoking_interval, std::string file_name, unsigned int file_size, unsigned int piece_size, bool has_file ) 
		: port_(port),
		my_peer_id_(peer_id),
		ip_(ip),
		has_file_(has_file),
		num_pref_neighbors_(num_pref_neighbors),
		unchoking_interval_(unchoking_interval),
		listening_sock_(0),
		file_name_(file_name),
		file_size_(file_size),
		piece_size_(piece_size) {

		total_pieces_ = ceiling_divide(file_size_, piece_size_);

		//TODO
		//this is implace for actually reading bits into the bitmap from the file
		int bytes = (total_pieces_ + 7) / 8;
		bitfield_.resize(bytes);
		std::fill(bitfield_.begin(), bitfield_.end(), 0xFF); //fills bitmap up with ones for testing

		
		
	}
	~P2P_Client() {
		if (listening_sock_ >= 0){ 
			stop_listening();
		}
		for (auto* n :neighbors_){
			delete n;
		}
	}

	int listen_on();
	int connect_to(std::string ip, uint16_t peer_port);
	bool send_message(uint8_t type, const void* payload, uint32_t payload_len, int sock);
	bool read_message(int sock);
	int start_communication();
	bool on_new_connection(int sock, std::string ip, uint16_t port, uint32_t peer_id);
	
	//handle types
	bool read_choke(int sock);
	bool read_unchoke(int sock);
	bool read_interested(int sock);
	bool read_uninterested(int sock);
	bool read_have(int sock, std::vector<char> buf);
	bool read_request(int sock, std::vector<char> buf);
	bool read_piece(int sock, std::vector<char> buf);
	bool read_bitfield(int sock, std::vector<char> buf);
	bool send_handshake(int sock, uint32_t peer_id);
	bool connect_and_handshake(std::string ip, uint16_t port);
	void accept_loop();
	bool read_handshake(int sock, std::string ip, uint16_t port);
	int start_listening();
	void stop_listening();
	void addNeighbor(int sock, std::string ip, uint16_t port, uint32_t peer_id);

	//getters
	uint32_t peer_id(){ return my_peer_id_;}
	std::vector<uint8_t> bitfield(){return bitfield_;}
	uint16_t port(){return port_;}
};

