#include "Client.hpp"
#include <map>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


int P2P_Client::create_listen_socket(){
	//Uses TCP, IPv4 (unsure if it should be IPv4)
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("failed while creating socket");
	}
	
	int opt = 1;
	int set_fd = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (set_fd < 0){
		perror("failed setting socket");
	}
	
	sockaddr_in addr{};
	addr.sin_family = AF_INET; //IPv4
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	
	if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0){
		perror("Failed to bind");
		close(fd);
		return -1;
	}

	if (listen(fd, 128) < 0){
		perror("Failed to listen");
		close(fd);
		return -1;
	}
	return fd;

}
