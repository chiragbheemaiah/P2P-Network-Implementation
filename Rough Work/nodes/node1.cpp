#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<cstring>
#include<arpa/inet.h>
#include<thread>
#include<chrono>
#include<cmath>

using namespace std;

class Server{
public:
	int server_fd;
	int port;
	int connections;
	Server(int port, int connections){
		this->port = port;
		this->connections = connections;
		
		this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if(server_fd == -1){
			perror("Connection could not be created\n");
			
		}
		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);
		int addrlen = sizeof(address);

		if(bind(server_fd, (struct sockaddr*)&address, addrlen) < 0){
			perror("Binding error");
			close(server_fd);
			//exit(1);
		}
	}
	
	void start(){
		if(listen(server_fd, this->connections) < 0){
			perror("Could not listen for connections");
			close(server_fd);
			//exit(1);
		}
		cout<<"Server 2 listening at port "<<port<<endl;
		while(true){
			struct sockaddr_in new_address;
			socklen_t new_addr_len = sizeof(new_address);
			int new_conn = accept(server_fd, (struct sockaddr*)&new_address, &new_addr_len);
			if(new_conn < 0){
				perror("Could not connect to client");
				//exit(1);
			}
			cout<<"New client has connected!"<<endl;
			char buffer[1024] = {0};

			int n = read(new_conn, buffer, 1024);

			if(n > 0){
				buffer[n] = '\0';
			}
			cout<<"{Client:}"<<buffer<<endl;

			const char* message = "Connected to server 2\n";
			write(new_conn, message, strlen(message));

			close(new_conn);
			
		}
		
		close(server_fd);
	}
};

class Client{
public:
	int client_fd;
	int port;
	Client(int port){
		client_fd = socket(AF_INET, SOCK_STREAM, 0);
		this->port = port;
		if(client_fd < 0){
			perror("Client socket intialization failed!");
			//exit(1);
		}	
	}
	
	
	void start(){
		struct sockaddr_in server_address;
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(port);
		inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

		
		const char* message = "Client 2 has connected!\n";
		
		// exponential backoff
		int i = 0;
		for(i = 0; i < 7; i ++){		
			if(connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
				perror("Connection client failed");
				//close(client_fd);
				//exit(1);
				this_thread::sleep_for(chrono::seconds(int(pow(2, i))));
			}else{
				break;
			}
		}
		if(i == 7){
			cout<<"Connection Failed, server not available"<<endl;
			close(client_fd);
			exit(1);
		}

		if(write(client_fd, message, strlen(message)) < 0){
			perror("Write failed!");
			close(client_fd);
			//exit(1);
		}

		char buffer[1024] = {0};
		int n = read(client_fd, buffer, 1024);
		if(n > 0)
			buffer[n] = '\0';
		cout<<"[SERVER] "<<buffer<<endl;
		
		close(client_fd);
	}
};

void start_server(){
	Server s(8001, 5);
	s.start();
}

void start_client(){
	Client c(8000);
	this_thread::sleep_for(chrono::seconds(10));
	c.start();	
}

int main(){
	thread t1(start_server);
	thread t2(start_client);
	t1.join();
	t2.join();
	return 0;
}


