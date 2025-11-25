#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <thread>
#include <chrono>

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
        return -1;
    }

    Config cfg;
    
    try{
        cfg = cfg.load(".");
    }
    catch (const std::exception& e){
        std::cerr << "[ERROR] Failed while loading config: " << e.what() << std::endl;
        return -1;
    }

    try {
        cfg.validateFilesystem(".", peerId);
    }
    catch(const std::exception& e){
        std::cerr << "[ERROR] Failed while validating Filesystem: " << e.what() << std::endl;
        return -1;
    }

    PeerRow* self = cfg.getPeer(peerId);
    if (!self) {
        std::cerr << "Peer ID " << peerId << " not found in PeerInfo.cfg" << std::endl;
        return -1;
    }
    
    std::vector<InitNeighborInfo> nInfo;
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

    std::cout << "Starting Peer " << peerId << "..." << std::endl;
    
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

    client.start_listening();
    
    std::cout << "Peer " << peerId << " is running on port " << myInfo.port << std::endl;
    std::cout << "Check log: log_peer_" << peerId << ".log" << std::endl;
    std::cout << "Press Ctrl+C to exit." << std::endl;

    std::cout << "Peer " << peerId << " running..." << std::endl;

	if (myInfo.hasFile) {
		std::cout << "This peer has the file. Press Ctrl+C to exit." << std::endl;
		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	} else {
		std::cout << "Waiting to download file..." << std::endl;
		while (!client.has_complete_file()) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		std::cout << "Download complete! Exiting..." << std::endl;
	}

    return 0;
}