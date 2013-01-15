#include "client.h"
#include <sstream>
#include "logger_decls.h"
#include <assert.h>

const int payload_length_known_offset = 8;
void set_non_blocking(int socket)
{
	assert(socket != -1);

	if (fcntl(socket, F_SETFL, fcntl(socket, F_GETFL) | O_NONBLOCK) < 0)
	{
		check_errno();
		log(log_error, "unable to set non-blocking mode on socket %d\n", socket);
	}
}

client_t::client_t(int socket, sockaddr_in *addr)
{
	set_non_blocking(socket);
	if (addr != NULL)
	{
		if (inet_ntop(AF_INET, &addr->sin_addr, ip_buffer, 32) != NULL)
		{
			ip_buffer[sizeof(ip_buffer) - 1] = '\0';
			std::stringstream ss;
			ss << socket << "|" << ip_buffer;
			info = ss.str();
		}
		else
		{
			dlog(log_warning, "inet_ntop failed");
		}
	}
}

bool client_t::read_complete(int socket)
{
	// NOTE: not bothering to think about htonl and friends here because
	// I demand all clients use little-endian byte ordering. 

	assert(bytes_read < sizeof(log_packet));
	uint8_t *pb = reinterpret_cast<uint8_t*>(&log_packet);
	int ret = 0;
	if (bytes_read < payload_length_known_offset)
	{
		errno = 0;
		ret = ::read(socket, pb + bytes_read, payload_length_known_offset - bytes_read);
		if (ret == 0)
		{
			bytes_read = 0;
			return true;
		}
		if (ret == -1)
		{
			if (errno == EAGAIN)
			{
				assert(0);
				return false;
			}
			check_errno();
			bytes_read = 0;
			return true;
		}

		bytes_read += ret;
	}

	if (bytes_read >= payload_length_known_offset)
	{
		const int total_packet_size = sizeof(log_packet) - sizeof(log_packet.payload) + log_packet.payload_length;
		ret = ::read(socket, pb + bytes_read, total_packet_size - bytes_read);
		if (ret == 0)
		{
			bytes_read = 0;
			return true;
		}
		if (ret == -1)
		{
			if (errno == EAGAIN)
				return false;
			check_errno();
			return true;
		}
		bytes_read += ret;
	}
	return complete();
}

bool client_t::complete()
{
	return bytes_read == payload_length_known_offset + log_packet.payload_length;
}

client_t::~client_t()
{
}
