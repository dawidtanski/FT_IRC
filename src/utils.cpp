#include "../include/utils.hpp"

int sendall(int sock_fd, char *buf, int *len)
{
	int total = 0;
	int bytesLeft = *len;
	int n = 0;

	while (total < *len)
	{
		n = send(sock_fd, buf+total, bytesLeft, 0);
		if (n == -1)
			break;
		total += n;
		bytesLeft -= n;
	}
	*len = total;

	return (n == -1 ? -1 : 0);
}

std::string inet_ntop2(const sockaddr_storage& addr)
{
	char buf[INET6_ADDRSTRLEN];

	if (addr.ss_family == AF_INET)
	{
		const sockaddr_in *sa4 = (const sockaddr_in *)&addr;
		if (!inet_ntop(sa4->sin_family, &sa4->sin_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else if (addr.ss_family == AF_INET6)
	{
		const sockaddr_in6 *sa6 = (const sockaddr_in6 *)&addr;
		if(!inet_ntop(sa6->sin6_family, &sa6->sin6_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else
		throw std::runtime_error("Couldn't convert address from binary to string");
}

void addToPollFDs(std::vector<struct pollfd>& pfds, int newFD)
{
	struct pollfd	pfd;

	pfd.fd = newFD;
	pfd.events = POLLIN;
	pfd.revents = 0;

	pfds.push_back(pfd);
}