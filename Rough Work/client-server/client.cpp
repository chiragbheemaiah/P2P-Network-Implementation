#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<cstring>
#include<arpa/inet.h>

using namespace std;

class Client{
	int client_fd;
public:
	Client(){
		this->client_fd = socket(AF_INET, SOCK_STREAM, 0);

		struct sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(8080);
		inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

		connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

		const char* msg = "Hello from Norris!\n";
		write(client_fd, msg, strlen(msg));

		char buffer[1024] = {0};
		int n = read(client_fd, buffer, 1024);
		if(n > 0){
			buffer[n] = '\0';
		}
		cout<<"{SERVER} "<<buffer<<endl;
		close(client_fd);
	}
};


int main(){
	Client c;
	return 0;
}
