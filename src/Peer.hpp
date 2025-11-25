#pragma once
#include <chrono>
#include <string>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "Neighbor.hpp"
#include "Header.hpp"
#include "logger.hpp"
#include <thread>
#include <atomic>
#include <unordered_map>
#include <set>
#include <fstream>

//helper struct for initializing clients neighbors
struct InitNeighborInfo {
	uint32_t peerId;
	std::string host;
	bool hasFile;
	uint16_t port;
};

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

	std::unordered_map<uint32_t, size_t> bytes_downloaded_from_;
	std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> last_download_time_;

	//piece tracking
	std::set<int> requested_pieces_;
	std::unordered_map<int, uint32_t> piece_to_peer_; //which peer requested which piece

	std::set<uint32_t> preferred_neighbors_;
	uint32_t optimistic_neighbor_;

	std::thread unchoke_thread_;
	std::thread optimistic_unchoke_timer_;
	std::vector<std::thread> peer_threads_;

	std::fstream file_;
	mutable std::mutex file_mu_;

	Logger* logger_;

	std::vector<Neighbor*> neighbors_;
	std::vector<uint8_t> bitfield_;

	std::unordered_map<uint32_t, bool> neighbor_has_file; //theres got to be a better way to do this
	std::unordered_map<int, uint32_t> sock_to_peer_;
	
	std::thread accept_thread_;
	std::atomic<bool> accepting_;
	std::mutex peers_mu_;

	std::atomic<bool> running_;
	void unchoke_timer_loop();
	void select_preferred_neighbors();

	Neighbor* find_neighbor_by_id(uint32_t id);
	Neighbor* find_neighbor_by_sock(int sock);

	void peer_message_loop(int sock);
	
public:
	//Constructor
	P2P_Client(uint32_t peer_id,
	    int port,std::string ip,
	    unsigned int num_pref_neighbors,
	    unsigned int unchoking_interval,
	    std::string file_name,
	    unsigned int file_size,
	    unsigned int piece_size,
	    bool has_file,
	    std::vector<InitNeighborInfo> neighbor_info
	    ) 
		: port_(port),
		my_peer_id_(peer_id),
		ip_(ip),
		has_file_(has_file),
		num_pref_neighbors_(num_pref_neighbors),
		unchoking_interval_(unchoking_interval),
		listening_sock_(0),
		file_name_(file_name),
		file_size_(file_size),
		piece_size_(piece_size),
		accepting_(false) {

		total_pieces_ = ceiling_divide(file_size_, piece_size_);
		
		for (const auto n : neighbor_info){
			connect_and_handshake(n.host, n.port, n.peerId, n.hasFile);
		}
		//TODO
		//this is implace for actually reading bits into the bitmap from the file

		//initialize bitfield
		if (has_file_){
			//all pieces are set to 1
			bitfield_.resize((total_pieces_ + 7)/8, 0xFF);
			int spare = (8 - (total_pieces_ % 8)) % 8;

			if (spare > 0){
				bitfield_.back() &= (0xFF << spare);
			}

		} else {
			bitfield_.resize((total_pieces_ + 7) / 8, 0x00);
    
			// check disk for pieces we might already have
			for (int i = 0; i < total_pieces_; i++) {
				if (has_piece_on_disk(i)) {
					set_bitfield_bit(i, true);
				}
			}
		}
	
		logger_ = new Logger("log_peer_" + std::to_string(my_peer_id_) + ".log");
		running_ = true;
		unchoke_thread_ = std::thread(&P2P_Client::unchoke_timer_loop, this);

		
		
	}
	~P2P_Client() {
		running_ = false;
		accepting_ = false;

		//clean up threads
		if (unchoke_thread_.joinable()){
			unchoke_thread_.join();
		}

		for (auto& t : peer_threads_){
			if (t.joinable()){
				t.join();
			}
		}

		if (accept_thread_.joinable()){
			accept_thread_.join();
		}
		
		// clean up sockets and neighbors
		if (listening_sock_ >= 0){ 
			stop_listening();
		}

		for (auto* n :neighbors_){
			delete n;
		}

		if (logger_){
			delete logger_;
			logger_ = nullptr;
		}
	}

	int listen_on();
	int connect_to(std::string ip, uint16_t peer_port);
	bool send_message(uint8_t type, const void* payload, uint32_t payload_len, int sock);
	bool read_message(int sock);
	int start_communication();
	bool on_new_connection(int sock, std::string ip, uint16_t port, uint32_t peer_id, bool has_file);
	
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
	bool connect_and_handshake(std::string ip, uint16_t port, int peer_id, bool has_file);
	void accept_loop();

	//overloading read handshake (one for when peer id is known before)
	bool read_handshake(int sock, std::string ip, uint16_t port, uint32_t expected_peer_id, bool has_file);
	bool read_handshake(int sock, std::string ip, uint16_t port);
	int start_listening();
	void stop_listening();
	void addNeighbor(int sock, std::string ip, uint16_t port, uint32_t peer_id, bool has_file);
	bool set_hasFile_from_bf(int sock, std::vector<char> buf);

	//helpers for file pieces
	bool read_piece_from_file(int piece_index, std::vector<char>& piece_data);
	bool write_piece_to_file(int piece_index, std::vector<char>& piece_data);
	bool has_piece_on_disk(int piece_index) const;
	void set_bitfield_bit(int piece_index, bool value);
	bool has_piece(int piece_index) const;
	bool has_complete_file() const;

	void start_peer_message_loop(int sock);

	void request_next_piece(int sock);





	//getters
	uint32_t peer_id(){ return my_peer_id_;}
	std::vector<uint8_t> bitfield(){return bitfield_;}
	uint16_t port(){return port_;}
};

