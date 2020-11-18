#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;

vector<int> client_list;

void usage() {
	cout << "syntax : echo-server <port> [-e[-b]]\n";
	cout << "sample : echo-server 1234 -e -b\n";
}

struct Param {
	bool autoNewline{false};
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-an") == 0) {
				autoNewline = true;
				continue;
			}
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				if (strcmp(argv[i], "-b") == 0) {
					broadcast = true;
					continue;
				}
				continue;
			}
			port = stoi(argv[i++]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	cout << "connected\n";
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			cerr << "recv return " << res << endl;
			perror("recv");
			break;
		}
		buf[res] = '\0';
		if (param.autoNewline)
			cout << buf << endl;
		else {
			cout << buf;
			cout.flush();
		}
		if (param.echo) {
			if (param.broadcast) {
				for (vector<int>::size_type i = 1; i < client_list.size(); i++){
					res = send(client_list[i], buf, res, 0);
					if (res == 0 || res == -1){
						cerr << "while broadcasting send return " << res << endl;
						perror("send");
						break;
					}
				}
			}
			else {
				res = send(sd, buf, res, 0);
				if (res == 0 || res == -1) {
					cerr << "send return " << res << endl;
					perror("send");
					break;
				}
			}
		}
	}
	for (vector<int>::size_type i = 1; i < client_list.size(); i++) {
		if (client_list[i] == sd) client_list.erase(client_list.begin()+i);
	}
	cout << "disconnected\n";
    close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int optval = 1;
	int res = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);

	ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res2 == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}

	while (true) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		int cli_sd = accept(sd, (struct sockaddr *)&cli_addr, &len);
		client_list.push_back(cli_sd);

		if (cli_sd == -1) {
			perror("accept");
			break;
		}
		thread* t = new thread(recvThread, cli_sd);
		t->detach();
	}
	close(sd);
}
