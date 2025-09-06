#include<iostream>
#include<chrono>
#include<thread>
#include<stdlib.h>
#include<vector>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<unistd.h>
#include<fcntl.h>
#include<cstring>
#include<string>
#include<math.h>

using namespace std;

/* 
 * A server - socket -> bind -> listen -> accept -> read/write -> close
 *
 * A client - socket -> connect ->read/write -> close
 *
 */

#define NUM_THREADS 4
int SERVER_PORT_NUMBER = 8082;
int CONNECTION_NUMBER = 5;
int NEIGHBOR_PORT_NUMBER = 8080;
string NEIGHBOR_SERVER_IP = "127.0.0.1";
bool source = false;
condition_variable queue_empty, queue_full;
string secret = "Apple";



class Client{
public:
	int client_fd;
	int port;
	string server_ip;
	Client(int port, string server_ip){
		this->client_fd = socket(AF_INET, SOCK_STREAM, 0);
		this->port = port;
		this->server_ip = server_ip;
		
		if(client_fd < 0){
			perror("Client socket intialization failed!");
			//exit(1);
		}	
	}
	
	
	void start_connection(){
		struct sockaddr_in server_address;
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(port);
		inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr);
		
		// exponential backoff
		int i = 0;
		for(i = 0; i < 7; i ++){		
			if(connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
				perror("Connection client failed");
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

	}
	
	string send_request(string msg){
		const char* message = msg.c_str();
		if(write(client_fd, message, strlen(message)) < 0){
			perror("Write failed!");
			close(client_fd);
		}

		char buffer[1024] = {0};
		int n = read(client_fd, buffer, 1024);
		if(n > 0)
			buffer[n] = '\0';
		cout<<"[SERVER] "<<buffer<<endl;
		string response(buffer);
		close(client_fd);
		return response;
	}
};


struct Task{
    int connection_id;
    struct sockaddr_in address;
    string request;
    Task() : connection_id(-1), address{}, request{} {}

    Task(int connection_id, struct sockaddr_in address, string request){
        this->connection_id = connection_id;
        this->address = address;
        this->request = request;
    }
};

mutex queue_mutex;
queue<Task> tasks;
int capacity = 10;
bool kill_switch = false; //unused as of now

class Server{
public: 
    int port, connections;
    int server_fd;
    Server(int port, int connections){
        this->port = port;
        this->connections = connections;

        this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(this->server_fd == -1){
            perror("Socket creation failed!");
        }


        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(this->port);
        socklen_t addrlen = sizeof(address);
        
        if(bind(this->server_fd, (struct sockaddr*)&address, addrlen) < 0){
            perror("Socket binding failed");
        }        
    }
    
    void start(){
    	if(listen(this->server_fd, this->connections) < 0){
            perror("Listening failed!");
        }
        cout<<"Server listening at "<<this->port<<endl;

        while(true){
            struct sockaddr_in new_address;
            socklen_t new_addr_len = sizeof(new_address);
            int new_conn = accept(this->server_fd, (struct sockaddr*)&new_address, &new_addr_len);
            cout<<"New connection established!"<<endl;

            char buffer[1024] = {0};

            int n = read(new_conn, buffer, 1024);
            
            if(n > 0){
                buffer[n] = '\0';
            }else{
                perror("Reading from client failed!");
            }
            cout<<"[CLIENT] "<<buffer<<endl;
	    	string request(buffer);
            Task task(new_conn, new_address, request);
            {
                unique_lock<mutex> lock(queue_mutex);
                queue_full.wait(lock, []{
                	return !(tasks.size() == capacity);
                });
                tasks.push(task);
            }
        	queue_empty.notify_one(); // how do you ensure thread rotation?
        }
    }
};


void executeTask(){
	while(true){
		Task task;
		{
			unique_lock<mutex> lock(queue_mutex);
			queue_empty.wait(lock, []{
				return !tasks.empty();
			});
			
			Task new_task = tasks.front();
			task = new_task;			
			tasks.pop();

		}
		queue_full.notify_one();
		
		// think about the connection descriptors - assumption - all are using different file descriptors - is there any other case?
		int connection_id = task.connection_id;
		string request = task.request;
		cout<<"Read request inside thread: "<<request<<endl;
		if(request == secret){
    			const char* successMessage = "Success";
			write(connection_id, successMessage, strlen(successMessage));
		}else{
			string probe = request;
			Client client(NEIGHBOR_PORT_NUMBER, NEIGHBOR_SERVER_IP);
			client.start_connection();
			string response = client.send_request(probe);
			if(response == "Success"){
				const char* successMessage = "Success";
				write(connection_id, successMessage, strlen(successMessage));
			}else{
				const char* errorMessage = "Not found";
				write(connection_id, errorMessage, strlen(errorMessage));
						
			}
			
			
		}
		close(connection_id);
	}

}


void start_server(){
	Server server(SERVER_PORT_NUMBER, CONNECTION_NUMBER);
	server.start();
}

void start_client(){
	if(source){
		string probe = "Apple";
		Client client(NEIGHBOR_PORT_NUMBER, NEIGHBOR_SERVER_IP);
		client.start_connection();
		cout<<"Client has started!"<<endl;
		cout<<"Client has sent request awaiting response!"<<endl;
		string response = client.send_request(probe);
		cout<<"Response received!"<<response<<endl;
		if(response == "Success"){
			cout<<"Probe successful, next steps, initiate a direct connection to the node, requires the IP of the node"<<endl;
		}else{
			cout<<"Probe failed, no such string exists in the system"<<endl;
		}
	}
}


int main(){
	vector<thread> threads;
	cout<<"Starting thread pool"<<endl;
	for(int i=0;i<NUM_THREADS; i++){
		thread t(executeTask);
		threads.push_back(move(t));
	}
	cout<<"Thread pool started"<<endl;

	//Server server(SERVER_PORT_NUMBER, CONNECTION_NUMBER);
	//server.start();
	
	thread server(start_server);
	cout<<"Server has started!"<<endl;
	thread client(start_client);
	
		
	
	
	
	for(int i=0;i<NUM_THREADS;i++){
		threads[i].join();
	}
	
	server.join();
	client.join();
    return 0;
}
