#include<iostream>
#include<string>
#include<cstring>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<stdlib.h>

using namespace std;

class Client{
public:
    int client_fd;
    string server_ip;
    int port;
    string name;

    Client(string server_ip, int port, string name){
        this->client_fd = socket(AF_INET, SOCK_STREAM, 0);
        this->server_ip = server_ip;
        this->port = port;
        this->name = name;

        if(this->client_fd < 0){
            perror("Socket creation failed");
        }

        struct sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(this->port);

        inet_pton(AF_INET, this->server_ip.c_str(), &server_address.sin_addr.s_addr);
        socklen_t addr_len = sizeof(server_address);
        
        if(connect(this->client_fd, (struct sockaddr*)&server_address, addr_len) < 0){
            perror("Connecting to the server failed!");
        }
        
        cout<<"Client "<<this->name<<" has connected to the server!"<<endl;

        string message = this->name;
        if(write(client_fd, message.c_str(), strlen(message.c_str())) < 0){
            perror("Sending request to server failed!");
        }

        char response[1024] = {0};
        int n = read(client_fd, response, 1024);
        if(n > 0){
            response[n] = '\0';
        }else{
            perror("Server response not received\n");
        }

        cout<<"Repsonse from the server: "<<response<<endl;

        close(client_fd);
    }
};

int main(int argc, char* argv[]){
    string client_name = argv[1];
    string server_ip = "127.0.0.1";
    int port = 8082;
    Client client(server_ip, port, client_name);
    return 0;
}
