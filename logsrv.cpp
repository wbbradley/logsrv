#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <assert.h>
#include <pthread.h>
#include "cmd_options.h"
#include "logger_decls.h"
#include "utils.h"
#include "client.h"
#include <tr1/unordered_set>

const char *option_port = "port";
const char *option_verbose = "verbose";
const char *option_backlog = "backlog";

cmd_option_t cmd_options[] =
{
	{ option_port, "-p" /*opt*/, true /*mandatory*/, true /*has_data*/ },
	{ option_backlog, "-b" /*opt*/, false /*mandatory*/, true /*has_data*/ },
	{ option_verbose, "-v" /*opt*/, false /*mandatory*/, false /*has_data*/ },
};

void set_sig_nopipe(int socket)
{
#ifdef SO_NOSIGPIPE
	int set = 1;
	errno = 0;
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, static_cast<void *>(&set), sizeof(set));
	if (!check_errno())
		log(log_warning, "failed setting no-SIGPIPE, continuing");
#endif
}

bool start_listening(int port, int backlog, int &socket)
{
	int socket_retval;
	errno = 0;
	socket_retval = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket_retval < 0)
	{
		log(log_error, "unable to create socket");
		return false;
	}
	socket = socket_retval;

	set_sig_nopipe(socket);

	int reuse_addr = 1;
	errno = 0;
	socket_retval = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	if (socket_retval != 0)
	{
		check_errno();
		log(log_warning, "setsockopt failed on this socket trying to enable SO_REUSEADDR, continuing...");
	}
	sockaddr_in socka;
	memset(&socka, 0, sizeof(socka));
	socka.sin_family = AF_INET;
	socka.sin_addr.s_addr = htonl(INADDR_ANY);
	socka.sin_port = htons(port);
	errno = 0;
	socket_retval = ::bind(socket, reinterpret_cast<sockaddr*>(&socka), sizeof(socka));
	if (socket_retval != 0)
	{
		check_errno();
		log(log_error, "port %d may be protected, try prefacing the command line with \"sudo\"\n", port);
		return false;
	}

	errno = 0;
	socket_retval = listen(socket, backlog);
	if (socket_retval != 0)
	{
		check_errno();
		return false;
	}

	log(log_info, "server started on port %d\n", port);

	return true;
}

bool accept_client(int socket, int &client_socket, shared_ptr<client_t> &client)
{
	while (true)
	{
		sockaddr_in client_addr;
		socklen_t client_addr_size = sizeof(client_addr);

		int socket_retval = -1;
		while (socket_retval == -1)
		{
			errno = 0;
			socket_retval = ::accept(socket,
					reinterpret_cast<sockaddr*>(&client_addr),
					&client_addr_size);
			check_errno();
		}

		if (socket_retval < 0)
			continue;
		dlog(log_info, "accepting client %s\n", inet_ntoa(client_addr.sin_addr));
		set_sig_nopipe(socket_retval);
		client_socket = socket_retval;
		client.reset(new client_t(client_socket, &client_addr));
		return client != nullptr;
	}

	/* not reached */
	return false;
}

template <typename T, typename U>
void start_thread(T pfn, U context)
{
	pthread_t pthread;
	pthread_create(&pthread, nullptr, pfn, (void *)context);
}

class log_server_t
{
public:
	void add_client(int client_socket, const shared_ptr<client_t> &client)
	{
		pthread_mutex_lock(&mutex);
		++version;
		assert(clients.find(client_socket) == clients.end());
		clients[client_socket] = client;
		pthread_mutex_unlock(&mutex);
	}

	void print_client_info(int socket, const shared_ptr<client_t> &client)
	{
		if (client->complete())
			fprintf(stdout, "LOG: [%s] [%s]\n", client->info.c_str(),
				   	std::string(client->log_packet.payload, client->log_packet.payload_length).c_str());
	}

	void process_clients()
	{
		uint64_t last_version = version - 1;
		std::vector<struct pollfd> pollfds;
		std::tr1::unordered_set<int> erase_set;
		std::vector<int> read_list;
		// C++11 - erase_set.reserve(100000);
		read_list.reserve(100000);
		while (true)
		{
			if (last_version != version)
			{
				pthread_mutex_lock(&mutex);
				last_version = version;
				pollfds.reserve(clients.size());
				pollfds.resize(0);
				for (auto &client : clients)
				{
					struct pollfd pollfd;
					pollfd.fd = client.first;
					pollfd.events = POLLRDNORM;
					pollfd.revents = 0;
					pollfds.push_back(pollfd);
				}
				pthread_mutex_unlock(&mutex);
			}

			read_list.resize(0);
			erase_set.clear();
			int ret = poll(&pollfds[0], pollfds.size(), 5 /*ms*/);
			if (ret > 0)
			{
				for (auto &pollfd : pollfds)
				{
					if (pollfd.revents & POLLHUP)
						erase_set.insert(pollfd.fd);

					if (pollfd.revents & POLLRDNORM)
						read_list.push_back(pollfd.fd);
				}
			}

			if (read_list.size() != 0)
			{
				pthread_mutex_lock(&mutex);
				for (int fd : read_list)
				{
					auto client_iter = clients.find(fd);
					if (client_iter != clients.end())
					{
						auto &client = client_iter->second;
						if (client->read_complete(fd))
						{
							erase_set.insert(fd);
							print_client_info(fd, client);
						}
					}
					else
					{
						assert(0);
					}
				}
				pthread_mutex_unlock(&mutex);
			}

			if (erase_set.size() != 0)
			{
				pthread_mutex_lock(&mutex);
				for (int fd : erase_set)
				{
					auto client_iter = clients.find(fd);
					if (client_iter != clients.end())
						clients.erase(client_iter);
					close(fd);
				}
				++version;
				pthread_mutex_unlock(&mutex);
			}
		}
	}

private:
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	clients_t clients;
	volatile uint64_t version = 0;
};

void *process_clients(void *context)
{
	pthread_detach(pthread_self());

	auto &log_server = (log_server_t &)*context;
	log_server.process_clients();
	return nullptr;
}

int main(int argc, char *argv[])
{
	cmd_options_t options;
	if (!get_options(argc, argv, cmd_options, countof(cmd_options), options))
		return EXIT_FAILURE;

	if (get_option_exists(options, option_verbose))
		log_enable(log_error | log_warning | log_info);
	else
		log_enable(log_error);

	int32_t port;
	get_option(options, option_port, port);
	int32_t backlog;
	if (!get_option(options, option_backlog, backlog))
		backlog = 1000;

	int socket = -1;
   	if (start_listening(port, backlog, socket))
	{
		log_server_t log_server;
		start_thread(process_clients, &log_server);
		while (true)
		{
			int client_socket = -1;
			shared_ptr<client_t> client;
			if (accept_client(socket, client_socket, client))
				log_server.add_client(client_socket, client);
		}
	}

	return EXIT_SUCCESS;
}

