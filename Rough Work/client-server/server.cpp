#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<cstring>

using namespace std;

class Server{
	int server_fd;
public:
	Server(){
		this->server_fd = socket(AF_INET, SOCK_STREAM, 0);

		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(8080);
		int addrlen = sizeof(address);

		bind(server_fd, (struct sockaddr*)&address, addrlen);

		listen(server_fd, 5);

		cout<<"Server lsitening on port 8080"<<endl;

		struct sockaddr_in new_address;
		char buffer[1024] = {0};
		socklen_t new_len = sizeof(new_address);
		int new_conn = accept(server_fd, (struct sockaddr*)&new_address, &new_len);

		ssize_t n = read(new_conn, buffer, sizeof(buffer)-1);
		if(n > 0) buffer[n] = '\0';

		cout<<"[CLIENT] : "<<buffer<<endl;
		const char* msg = "Hello from the server\n";
		write(new_conn, msg, strlen(msg));

		close(new_conn);
		close(server_fd);



	}
};
int main(){
	Server s;
	return 0;
}
