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
#include<sstream>

using namespace std;

// CONFIGURATIONS
#define NUM_THREADS 4
int SERVER_PORT_NUMBER = 8090;
int CONNECTION_NUMBER = 5;
int NEIGHBOR_PORT_NUMBER = 8091;
string NEIGHBOR_SERVER_IP = "127.0.0.1";
string SELF_IP = "127.0.0.1";
bool source = true;
string secret = "Thor";
int HOP_COUNT = 5;

struct Message{
	string content;
	int hop_count;
	string source_ip;
	string destination_ip;
	int destination_port_number;

	Message() : content(""), hop_count(0), source_ip(""), destination_ip(""), destination_port_number(-1) {}

	Message(string content, int hop_count, string source_ip, string destination_ip, int destination_port_number){
		this->content = content;
		this->hop_count = hop_count;
		this->source_ip = source_ip;
		this->destination_ip = destination_ip;
		this->destination_port_number = destination_port_number;
	}

	string serialize(){
		return content + "#" + to_string(hop_count) + "#" + source_ip + "#" + destination_ip + "#" + to_string(destination_port_number);
	}

	static Message deserialize(const string& message){
		stringstream ss(message);
		vector<string> tokens;
		const char delimiter = '#';
		string item;
		while(getline(ss, item, delimiter)){
			tokens.push_back(item);
		}
		if(tokens.size() != 5){
			perror("Wrong message format!");
		}

		string content = tokens[0];
		int hop_count = stoi(tokens[1]);
		string src_ip = tokens[2];
		string dest_ip = tokens[3];
		int destination_port_number = stoi(tokens[4]);

		return Message(content, hop_count, src_ip, dest_ip, destination_port_number);
	}
};


// CLIENT
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
	
	string send_request(Message msg){
		string res = msg.serialize();
		if(write(client_fd, res.c_str(), res.size()) < 0){
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

// FOR THREADS TO ACCEPT CONNECTIONS AND ASSOCIATE THE CONNECTION_FD
struct Task{
    int connection_id;
    struct sockaddr_in address;
    Message request;
    Task() : connection_id(-1), address{}, request{} {}

    Task(int connection_id, struct sockaddr_in address, Message request){
        this->connection_id = connection_id;
        this->address = address;
        this->request = request;
    }
};

// LOCKING MECHANISM
mutex queue_mutex;
queue<Task> tasks;
int capacity = 10;
bool kill_switch = false; //TODO: SHUTDOWN NEEDS TO BE CONFIGURED
condition_variable queue_empty, queue_full;


//MULTITHREADED SERRVER IMPLEMENTATION WITH AN ACCEPT QUEUE
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

        int opt = 1;
		if (setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		    perror("setsockopt(SO_REUSEADDR) failed");
		}

        
        if(bind(this->server_fd, (struct sockaddr*)&address, addrlen) < 0){
            perror("Socket binding failed");
        }        
    }
    
    void start(){
    	if(listen(this->server_fd, this->connections) < 0){
            perror("Listening failed!");
        }
        cout<<"[INFO] : Server listening at "<<this->port<<endl;

        while(true){
            struct sockaddr_in new_address;
            socklen_t new_addr_len = sizeof(new_address);
            int new_conn = accept(this->server_fd, (struct sockaddr*)&new_address, &new_addr_len);
            cout<<"[INFO] : New connection established!"<<endl; //TODO: ADD IP INFORMATION

            char buffer[1024] = {0};
            int n = read(new_conn, buffer, 1024);
            if(n > 0){
                buffer[n] = '\0';
            }else{
                perror("Reading from client failed!");
            }
            cout<<"[CLIENT] : Looking for "<<buffer<<endl; // TODO: ADD IP INFOMRATION
	    	string request(buffer);

	    	Message msg = Message::deserialize(request);

            Task task(new_conn, new_address, msg);
            {
                unique_lock<mutex> lock(queue_mutex);
                queue_full.wait(lock, []{
                	return !(tasks.size() == capacity);
                });
                tasks.push(task);
            }
	    	queue_empty.notify_one(); // QUESTIONS: How do you ensure thread rotation?
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
		
		// QUESTIONS: think about the connection descriptors - assumption - all are using different file descriptors - is there any other case?
		int connection_id = task.connection_id;
		Message request = task.request;
		cout<<"[INFO] : Read request inside thread from the client: "<<request.content<<endl; //TODO: ADD THREAD_ID
		
		// DECREMENTING HOP COUNT
		request.hop_count  -= 1;

		const string successMessage = "Success";
		const string errorMessage = "Not found";

		// IF THE CURRET SERVER HAS THE MESSAGE, THEN SET CONTENT TO SUCCESS AND SEND IT BACK
		if(request.content == secret){
			Message msg(successMessage, request.hop_count, request.source_ip, SELF_IP, SERVER_PORT_NUMBER);
			string response = msg.serialize();
			write(connection_id, response.c_str(), response.size());

		// ELSE CHECK THE MESSAGES HOP COUNT, IF THE HOP_COUNT = 0, SET NOT FOUND AND RETURN, ELSE CREATE A NEW CLIENT 
		// AND SEND A REQUEST TO THE NEIGHBOR AND FORWARD THE RESPONSE BACK TO THE CLIENT MAKING THE REQUEST.	
		}else{
			if(request.hop_count != 0){
				Client client(NEIGHBOR_PORT_NUMBER, NEIGHBOR_SERVER_IP);
				client.start_connection();
				string response = client.send_request(request);
				write(connection_id, response.c_str(), response.size());
			}else{
				Message msg(errorMessage, request.hop_count, request.source_ip, SELF_IP, SERVER_PORT_NUMBER);
				string response = msg.serialize();
				write(connection_id, response.c_str(), response.size());				
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

		string probe = "Banana";
		Message message(probe, HOP_COUNT, SELF_IP, "", -1);

		Client client(NEIGHBOR_PORT_NUMBER, NEIGHBOR_SERVER_IP);
		client.start_connection();
		cout<<"[INFO] : Client has started!"<<endl;

		cout<<"[INFO] : Client sending request and awaiting response!"<<endl;
		string response = client.send_request(message);

		cout<<"[INFO] : Response received!"<<response<<endl;
		Message msg = Message::deserialize(response);

		if(msg.content == "Success"){
			cout<<"Probe successful, next steps, initiate a direct connection to the node, requires the IP of the node"<<endl;
		}else{
			cout<<"Probe failed, no such string exists in the system"<<endl;
		}
	}
}


int main(){
	cout<<"[INFO] : Starting thread pool"<<endl;

	vector<thread> threads;
	for(int i=0;i<NUM_THREADS; i++){
		thread t(executeTask);
		threads.push_back(move(t));
	}
	
	cout<<"[INFO] : Thread pool initialized"<<endl;
	
	thread server(start_server);
	cout<<"[INFO] : Server has started!"<<endl;
	thread client(start_client);
		
	
	for(int i=0;i<NUM_THREADS;i++){
		threads[i].join();
	}
	
	server.join();
	client.join();
	
	cout<<"[INFO] : Program terminating, all the threads completed execution!"<<endl;
    return 0;
}
