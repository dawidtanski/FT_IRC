#include "include/irc.hpp"
#include "include/Server.hpp"

int main(int argc, char **argv)
{
	int			port;
	std::string	password;

	if (argc != 3)
	{
		std::cout << "Usage:\n ./ircserv <port> <password>" << std::endl;
		return (1);
	}

	port = std::atoi(argv[1]);
	password = argv[2];

	// === PRINT INPUT ===
	std::cout << "port:\t\t" << port << "\npassword:\t" << password << std::endl;
	// ===================
	
	// ========= MAIN PART ========

	try
	{
		Server server(port, argv[1], password);
		server.start();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}