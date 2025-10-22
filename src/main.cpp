#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include "config.h"
#include "Peer.hpp"
#include "Neighbor.hpp"

	

int main(int argc, char *argv[]){
	if (argc < 2){
		std::cout << "Usage: peerProcess <peer ID>" << std::endl;
		return 0;
	}

	int peerId = 0;

	try {
		peerId = std::stoi(argv[1]);
	}
	catch(...){
		std::cerr << "[ERROR] Invalid peerId (must be numeric)" << std::endl;
	}

	Config cfg;
	
	try{
		cfg = cfg.load(".");
	}
	catch (std::exception e){
		std::cerr << "[ERROR] Failed while loading config: " << e.what() << std::endl;
		return -1;
	}

	try {
		cfg.validateFilesystem(".", peerId);
	}
	catch(std::exception e){
		std::cerr << "[ERROR] Failed while validating Filesystem: " << e.what() << std::endl;
		return -1;
	}
	cfg.validateFilesystem(".", peerId);

	PeerRow* self = cfg.getPeer(peerId);
	if (!self) {
		std::cerr << "Peer ID " << peerId << " not found in PeerInfo.cfg" << endl;
		return -1;
	}
	std::vector<InitNeighborInfo> nInfo;//used to load all peers that come before it in config file
	InitNeighborInfo myInfo;
	for (const auto& p : cfg.peers) {
		InitNeighborInfo n;
		n.hasFile = p.hasFile;
		n.host = p.host;
		n.port = p.port;
		n.peerId = p.id;
		
		if (p.id == peerId){
			myInfo = n;
			break;
		}

		nInfo.push_back(n);
	}


	
	P2P_Client client(
        	myInfo.peerId,
        	myInfo.port,
        	myInfo.host,
        	cfg.common.preferredNeighbors,
        	cfg.common.unchokeIntervalSec,
        	cfg.common.fileName,
        	cfg.common.fileSizeBytes,
        	cfg.common.pieceSizeBytes,
        	myInfo.hasFile,
		nInfo

    	);

	return 0;
}
