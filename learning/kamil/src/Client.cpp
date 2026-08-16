#include "../include/Client.hpp"

Client::~Client(){
	if (_fd >= 0)
		close(_fd);
}