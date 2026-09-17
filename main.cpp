#include "include/Server.hpp"

int main(int argc, char **argv) {
	if (argc != 3) {
		std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
		return 1;
	}
	try {
		const std::string port(argv[1]);
		const std::string password(argv[2]);
		if (port.empty() || port.size() > 5 || port.find_first_not_of("0123456789") != std::string::npos
			|| std::atoi(argv[1]) < 1 || std::atoi(argv[1]) > 65535)
			throw std::runtime_error("Port must be a number from 1 to 65535");
		if (password.empty() || password.size() > 128 || password.find_first_of(" \t\r\n\v\f") != std::string::npos)
			throw std::runtime_error("Password must contain 1-128 characters without whitespace");
		Server server(argv[1], password);
		server.start();
	} catch (const std::exception &e) {
		std::cerr << "ircserv: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
