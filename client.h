#pragma once
#include <string>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utils.h"
#include <tr1/unordered_map>

struct log_packet_t
{
	uint8_t version;
	uint16_t message_type;
	uint32_t user_id;
	uint8_t payload_length;
	char payload[256];
} __attribute ((packed));

struct client_t
{
	client_t(int socket, sockaddr_in *addr);
	~client_t();

	bool read_complete(int socket);
	bool complete();

	char ip_buffer[32];
	std::string info;
	int bytes_read = 0;
	log_packet_t log_packet;
};

typedef std::tr1::unordered_map<int, shared_ptr<client_t> > clients_t;
