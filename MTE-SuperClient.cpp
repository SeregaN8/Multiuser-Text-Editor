#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include <sstream>
#include <set>
const int PORT = 37641;
const int buffer_size = 10'000'000;

std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
std::set<std::string> whitelist;

int get_ip(char* str) {
	if (!('0' <= *str && *str <= '9')) {
		std::cout << "Invalid ip format, Incorrect ip, the programme will"
			" select default ip value (127.0.0.1)\n";
		return 0x7f000001;
	}

	unsigned int read{}, ret{};
	for (int i = 0; i < 4; ++i) {
		while (*str >= '0' && *str <= '9') {
			read *= 10, read += *str - '0', ++str;
			if (read > 255) {
				std::cout << "Invalid ip format, Incorrect ip, the programme will"
					" select default ip value (127.0.0.1)\n";
				return 0x7f000001;
			}
		}
		ret *= 256, ret += read, read = 0;
		if (i < 3) {
			if (*str != '.') {
				std::cout << "Invalid ip format, Incorrect ip, the programme will"
					" select default ip value (127.0.0.1)\n";
				return 0x7f000001;
			}
			else {
				++str;
				if (!('0' <= *str && *str <= '9')) {
					std::cout << "Invalid ip format, Incorrect ip, the programme will"
						" select default ip value (127.0.0.1)\n";
					return 0x7f000001;
				}
			}
		}
	}
	return ret;
}

SOCKET create_connection(unsigned int ip_to_connect) {
	int status;
	SOCKET client_fd;
	sockaddr_in serv_addr;
	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	serv_addr.sin_addr.s_addr = htonl(ip_to_connect);

	if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
		return INVALID_SOCKET;
	}
	return client_fd;
}

void write_help_info(const std::string& admin_key, int ip) {
	std::cout << "This is super-client (aka admin) for Mte project\n"
		"Current admin key to connect is " << admin_key << " ,\n"
		"current server ip (in uint format) is considered " << ip << " .\n"
		"You can use next commands: \n"
		"\'help\' - prints this info text one more time.\n"
		"\'add <username>\' - sends to server a message to add <username> into whitelist.\n"
		"    Usernames should be no longer than 20 symbols.\n"
		"\'whitelist\' - prints current whitelist.\n"
		"\'stop\' - immediatly sends to server termination command.\n";
}

void write_current_whitelist() {
	if (whitelist.empty()) {
		std::cout << "Current whitelist is empty\n";
		return;
	}
	std::cout << "Current whitelist contains " << whitelist.size() << " names:\n";
	for (const auto& name : whitelist) std::cout << name << '\n';
}

void socket_creation_failure() {
	std::cout << "Unable to connect; do you want to wait or close the client right now?\n"
		"In case of closing you will be able to send stop command later, but you will\n"
		"loose current whitespace\n"
		"Enter Y/n if you want to exit the client right now\n";
	char confirmation{};
	std::cin >> confirmation;
	if (confirmation == 'Y') {
		std::exit(0);
	}
	else if (confirmation != 'n') {
		std::cout << "Entered something else, considered waiting\n";
	}
}

void send_stop(const std::string& admin_key, int ip) {
	SOCKET client_fd = create_connection(ip);
	if (client_fd == INVALID_SOCKET) {
		socket_creation_failure();
		return;
	}
	char* to_send = new char[admin_key.length() + 3];
	int pos{};
	memcpy(to_send, admin_key.c_str(), admin_key.size() + 1), pos += admin_key.size() + 1;
	memcpy(to_send + pos, "s", 2), pos += 2;
	send(client_fd, to_send, pos, 0);
	closesocket(client_fd);
	delete[] to_send;
	std::exit(0);
}

void send_add(const std::string& name, const std::string& admin_key, int ip) {
	SOCKET client_fd = create_connection(ip);
	if (client_fd == INVALID_SOCKET) {
		socket_creation_failure();
		return;
	}
	char* to_send = new char[admin_key.length() + 4 + name.length()];
	int pos{};
	memcpy(to_send, admin_key.c_str(), admin_key.size() + 1), pos += admin_key.size() + 1;
	memcpy(to_send + pos, "a", 2), pos += 2;
	memcpy(to_send + pos, name.c_str(), name.length() + 1), pos += name.length() + 1;
	send(client_fd, to_send, pos, 0);
	char* buf = new char[buffer_size];
	recv(client_fd, buf, buffer_size, 0);
	closesocket(client_fd);
	std::cout << "Server comment: \n";
	pos = 0;
	while (buf[pos]) std::cout << buf[pos++];
	std::cout << '\n';
	delete[] buf;
	delete[] to_send;
}

void parse(const std::string& command, const std::string& admin_key, int ip) {
	if (command == "help") {
		write_help_info(admin_key, ip);
		return;
	}
	if (command == "whitelist") {
		write_current_whitelist();
		return;
	}
	if (command == "stop") {
		send_stop(admin_key, ip);
		return;
	}
	std::stringstream parsing(command);
	std::string cmd, name;
	parsing >> cmd >> name;
	if (cmd != "add") {
		std::cout << "Uknown command\n";
		return;
	}
	if (name.empty()) {
		std::cout << "Expected a name in addition to command \'add\'\n";
		return;
	}
	send_add(name, admin_key, ip);
	whitelist.insert(name);
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cout << "Expected 2 arguments: ip and admin get, recieved " << argc - 1 << '\n';
		return 0;
	}
	unsigned int server_ip = get_ip(argv[1]);
	std::string admin_key(argv[2]);

	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		std::cerr << "WSA startup failed\n";
		std::exit(1);
	}
	std::string command;

	write_help_info(admin_key, server_ip);

	while (std::getline(std::cin, command)) {
		parse(command, admin_key, server_ip);
	}
}