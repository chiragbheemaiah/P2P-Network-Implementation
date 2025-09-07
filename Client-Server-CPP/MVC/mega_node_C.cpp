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
#include <cerrno>

using namespace std;

// CONFIGURATIONS
struct Configurations{
	//defaults
	int NUM_THREADS;
	int SERVER_PORT_NUMBER;
	int CONNECTION_LIMIT;
	int NEIGHBOR_PORT_NUMBER;
	string NEIGHBOR_SERVER_IP;
	string SERVER_IP;
	bool IS_SOURCE;
	string SECRET;
	string QUERY_STRING;
	int HOP_COUNT;
} config;

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
			cerr << "[ERROR] : Wrong message format! "<<endl;
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
	string id;
	Client(int port, string server_ip, string id){
		this->client_fd = socket(AF_INET, SOCK_STREAM, 0);
		this->port = port;
		this->server_ip = server_ip;
		this->id = id;

		if(client_fd < 0){
			cerr << "[ERROR] : Client - " << this->id << " Client socket intialization failed! " << strerror(errno) << endl;
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
				cerr << "[ERROR] : Client - " << this->id << " Connection client failed " << strerror(errno) << endl;
				this_thread::sleep_for(chrono::seconds(int(pow(2, i))));
			}else{
				break;
			}
		}
		if(i == 7){
			cerr<<"[ERROR] : Client - "<<this->id<<" Connection Failed, server not available"<<endl;
			close(client_fd);
			exit(1);
		}

	}
	
	string send_request(Message msg){
		string res = msg.serialize();
		if(write(client_fd, res.c_str(), res.size()) < 0){
			cerr << "[ERROR] : Client - "<<this->id<<"Write failed!" << strerror(errno) << endl;
			close(client_fd);
		}

		char buffer[1024] = {0};
		int n = read(client_fd, buffer, 1024);
		if(n > 0)
			buffer[n] = '\0';
		cout<<"[INFO] : Client - "<<this->id<<"Message received in raw from the server: "<<buffer<<endl;
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


//MULTITHREADED SERRVER IMPLEMENTATION WITH A TASKS QUEUE
class Server{
public: 
    int port, connections;
    int server_fd;
    string id;
    Server(int port, int connections, string id){
        this->port = port;
        this->connections = connections;
        this->id = id;
        this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(this->server_fd == -1){
            cerr << "[ERROR] : Server - "<<id<<" Socket creation failed! " << strerror(errno) << endl;
        }


        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(this->port);
        socklen_t addrlen = sizeof(address);

        int opt = 1;
		if (setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		    cerr << "[WARNING] : Server - "<<this->id<<" setsockopt(SO_REUSEADDR) failed " << strerror(errno) << endl;
		}

        
        if(bind(this->server_fd, (struct sockaddr*)&address, addrlen) < 0){
            cerr << "[WARNING] : Server - "<<this->id<<" Socket binding failed " << strerror(errno) << endl;
        }        
    }
    
    void start(){
    	if(listen(this->server_fd, this->connections) < 0){
            cerr << "[ERROR] : Server - "<<this->id<<" Listening failed! " << strerror(errno) <<endl;
        }
        cout<<"[INFO] : Server - "<<this->id<<" listening at "<<this->port<<endl;

        while(true){
            struct sockaddr_in new_address;
            socklen_t new_addr_len = sizeof(new_address);
            int new_conn = accept(this->server_fd, (struct sockaddr*)&new_address, &new_addr_len);

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(new_address.sin_addr), client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(new_address.sin_port);
            cout<<"[INFO] : Server - "<<this->id<<" : New connection established from IP "<<client_ip<<" on port "<<client_port<<endl;

            char buffer[1024] = {0};
            int n = read(new_conn, buffer, 1024);
            if(n > 0){
                buffer[n] = '\0';
            }else{
                cerr <<"[ERROR] : Server - "<<this->id<<" Reading from client failed! " << strerror(errno) << endl;
            }
            cout<<"[INFO] : Server - "<<this->id<<" Message received raw from client: "<<buffer<<endl;
	    	
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


void executeTask(string id){
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
		cout<<"[INFO] : Server - " << id <<" Read request from the client inside server's thread - " <<this_thread::get_id() <<" from the client: "<<request.content<<endl; //TODO: ADD THREAD_ID, ADD SERVER_ID
		
		// DECREMENTING HOP COUNT
		request.hop_count  -= 1;

		const string successMessage = "Success";
		const string errorMessage = "Not found";

		// IF THE CURRET SERVER HAS THE MESSAGE, THEN SET CONTENT TO SUCCESS AND SEND IT BACK
		if(request.content == config.SECRET){
			Message msg(successMessage, request.hop_count, request.source_ip, config.SERVER_IP, config.SERVER_PORT_NUMBER);
			string response = msg.serialize();
			write(connection_id, response.c_str(), response.size());

		// ELSE CHECK THE MESSAGES HOP COUNT, IF THE HOP_COUNT = 0, SET NOT FOUND AND RETURN, ELSE CREATE A NEW CLIENT 
		// AND SEND A REQUEST TO THE NEIGHBOR AND FORWARD THE RESPONSE BACK TO THE CLIENT MAKING THE REQUEST.	
		}else{
			if(request.hop_count != 0){
				Client client(config.NEIGHBOR_PORT_NUMBER, config.NEIGHBOR_SERVER_IP, id);
				client.start_connection();
				string response = client.send_request(request);
				write(connection_id, response.c_str(), response.size());
			}else{
				Message msg(errorMessage, request.hop_count, request.source_ip, config.SERVER_IP, config.SERVER_PORT_NUMBER);
				string response = msg.serialize();
				write(connection_id, response.c_str(), response.size());				
			}	
		}
		close(connection_id);
	}

}

void start_server(string ID){
	Server server(config.SERVER_PORT_NUMBER, config.CONNECTION_LIMIT, ID);
	server.start();
}

void start_client(string ID){
	if(config.IS_SOURCE){

		string probe = config.QUERY_STRING;
		Message message(probe, config.HOP_COUNT, config.SERVER_IP, "", -1);

		Client client(config.NEIGHBOR_PORT_NUMBER, config.NEIGHBOR_SERVER_IP, ID);
		client.start_connection();
		cout<<"[INFO] : Client has started!"<<endl;

		cout<<"[INFO] : Client sending request and awaiting response!"<<endl;
		string response = client.send_request(message);

		cout<<"[INFO] : Response received! "<<response<<endl;
		Message msg = Message::deserialize(response);

		if(msg.content == "Success"){
			cout<<"Probe successful, next steps, initiate a direct connection to the node, requires the IP of the node"<<endl;
		}else{
			cout<<"Probe failed, no such string exists in the system"<<endl;
		}
	}
}


int main(int argc, char* argv[]){
	if(argc < 12){
		cout<<
			"Usage: <executable> <ID> <NUM_THREADS> <SERVER_IP> <SERVER_PORT_NUMBER>" <<
			"<CONNECTION_NUMBER> <NEIGHBOR_SERVER_IP> <NEIGHBOR_PORT_NUMBER> " << 
			"<IS_SOURCE:BOOL> <SECRET> <QUERY_STRING> <HOP_COUNT>, Please provide all the required arguments to start the server."
			<<endl;
		return -1;
	}
	
	string id(argv[1]);

	config.NUM_THREADS = stoi(argv[2]);
	config.SERVER_IP =argv[3];
	config.SERVER_PORT_NUMBER = stoi(argv[4]);
	config.CONNECTION_LIMIT = stoi(argv[5]);
	config.NEIGHBOR_SERVER_IP = argv[6];
	config.NEIGHBOR_PORT_NUMBER = stoi(argv[7]);
	config.IS_SOURCE = false;
	string is_source(argv[8]);
	if(is_source == "true"){
		config.IS_SOURCE = true;
	}
	config.SECRET = argv[9];
	config.QUERY_STRING = argv[10];
	config.HOP_COUNT = stoi(argv[11]);

	cout<<"[INFO] : Server - "<<id<<" Starting thread pool"<<endl;

	vector<thread> threads;
	for(int i = 0; i < config.NUM_THREADS; i++){
		thread t(executeTask, id);
		threads.push_back(move(t));
	}
	
	cout<<"[INFO] : Server - "<<id<<" Thread pool initialized"<<endl;
	
	thread server(start_server, id);
	cout<<"[INFO] : Server - "<<id<<" has started!"<<endl;
	thread client(start_client, id);
		
	
	for(int i = 0; i < config.NUM_THREADS; i++){
		threads[i].join();
	}
	
	server.join();
	client.join();
	
	cout<<"[INFO] : Program terminating, all the threads have completed execution!"<<endl;
    return 0;
}
