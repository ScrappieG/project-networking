#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unistd.h>

class Neighbor{
private:
	uint16_t port_; // we cant make this const because we have to change it to -1 in assignment operator

	int sock_;
	int num_pieces_;
	std::string ip_;

	bool choked_;
	bool interested_;
	std::vector<uint8_t> bitfield_;//stores a byte per index

public:
	Neighbor(int sock, uint16_t port, int num_pieces, std::string ip)
		: port_(port),
		sock_(sock),
		num_pieces_(num_pieces), 
		ip_(std::move(ip)),
		choked_(true),
		interested_(false),
		bitfield_((num_pieces+7) / 8, 0){}

	~Neighbor() {
		if (sock_ >= 0){
			close(sock_);
			sock_ = -1;
		}
	}

	// this is only here because we probably shouldn't copy neighbors
	// if you copy a neighbor it will attempt to close the same socket twice
	
	Neighbor(const Neighbor&) = delete;
	Neighbor& operator = (const Neighbor&) = delete;

	//move
	//this looks terrible but it just allows for moving a neighbor to another variable without the possibility of closing the same socket twice
	//ie makes it harder to shoot yourself in the foot
	Neighbor(Neighbor&& other) noexcept
		:port_(other.port_),
		sock_(other.sock_),
		num_pieces_(other.num_pieces_),
		choked_(other.choked_),
		interested_(other.interested_),
		bitfield_(std::move(other.bitfield_)){
		
		other.sock_ = -1;
		other.port_ = 0;
		other.num_pieces_ = 0;
		other.choked_ = true;
		other.interested_ = false;
	}

	//overrights assignment operator (again so that we dont shoot ourselves in the foot c++ :D)
	
	Neighbor& operator=(Neighbor&& other) noexcept {
		if (this != &other) {
			if (sock_ >= 0) {
				close(sock_);
			}
			port_ = other.port_;
			sock_ = other.sock_;
			num_pieces_ = other.num_pieces_;
			ip_ = std::move(other.ip_);
			choked_ = other.choked_;
			interested_ = other.interested_;
			bitfield_ = std::move(other.bitfield_);
			other.sock_ = -1;
			other.port_ = 0;
			other.num_pieces_ = 0;
			other.choked_ = true;
			other.interested_ = false;
		}
		return *this;
	}
	
	//getters
	int sock() const { return sock_;}
	uint16_t port() const {return port_;}
	bool choked() const { return choked_; }
	bool interested() const { return interested_; }
	std::vector<uint8_t> bitfield() const { return bitfield_; }

	//setters
	void set_interested(bool val){ this->interested_ = val;}
	void set_choked(bool val){ this->choked_ = val; }
	

};
