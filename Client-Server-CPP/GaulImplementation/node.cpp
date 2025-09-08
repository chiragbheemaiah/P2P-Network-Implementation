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
#include<random>
#include<time.h>

using namespace std;

vector<string> ITEMS = {"fish", "boar", "salt"};

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
	int QTY;
	string QUERY_STRING;
	int HOP_COUNT;
} config;

class IMessage{
public:
	string type;
	string content;
	string source_ip;
	string destination_ip;
	int destination_port_number;
	int hop_count;

	IMessage() : type(""), content(""), source_ip(""), hop_count(-1), destination_ip(""), destination_port_number(-1) {}

	IMessage(string type, string content, string source_ip, int hop_count, string destination_ip, int destination_port_number){
		this->type = type;
		this->content = content;
		this->source_ip = source_ip;
		this->hop_count = hop_count;
		this->destination_ip = destination_ip;
		this->destination_port_number = destination_port_number;
	}
	virtual string serialize() = 0;
};

class BuyMessage : public IMessage{
public:

	BuyMessage() : IMessage() {}

	BuyMessage(string type, string content, string source_ip, string destination_ip, int destination_port_number) : IMessage(type, content, source_ip, -1, destination_ip, destination_port_number){}

	string serialize() override{
		return type + "#" + content + "#" + source_ip + "#" + destination_ip + "#" + to_string(destination_port_number);
	}

	static IMessage* deserialize (const string& buy_message){
		stringstream ss(buy_message);
		vector<string> tokens;
		const char delimiter = '#';
		string item;
		while(getline(ss, item, delimiter)){
			tokens.push_back(item);
		}
		if(tokens.size() != 5){
			cerr << "[ERROR] : Wrong buy_message format! "<<endl;
		}
		string type = tokens[0];
		string content = tokens[1];
		string src_ip = tokens[2];
		string dest_ip = tokens[3];
		int destination_port_number = stoi(tokens[4]);

		IMessage* msg = new BuyMessage(type, content, src_ip, dest_ip, destination_port_number);
		return msg;
	}
};

class QueryMessage : public IMessage{
public:
	QueryMessage() : IMessage(){}

	QueryMessage(string type, string content, int hop_count, string source_ip, string destination_ip, int destination_port_number): IMessage(type, content, source_ip, hop_count, destination_ip, destination_port_number){
		this->hop_count = hop_count;
	}

	string serialize() override{
		return type + "#" + content + "#" + to_string(hop_count) + "#" + source_ip + "#" + destination_ip + "#" + to_string(destination_port_number);
	}

	static IMessage* deserialize(const string& query_message){
		stringstream ss(query_message);
		vector<string> tokens;
		const char delimiter = '#';
		string item;
		while(getline(ss, item, delimiter)){
			tokens.push_back(item);
		}
		if(tokens.size() != 6){
			cerr << "[ERROR] : Wrong query_message format! "<<endl;
		}
		string type = tokens[0];
		string content = tokens[1];
		int hop_count = stoi(tokens[2]);
		string src_ip = tokens[3];
		string dest_ip = tokens[4];
		int destination_port_number = stoi(tokens[5]);

		IMessage* msg = new QueryMessage(type, content, hop_count, src_ip, dest_ip, destination_port_number);
		return msg;
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
	
	string send_request(IMessage* msg){
		string res = msg->serialize();
		if(write(client_fd, res.c_str(), res.size()) < 0){
			cerr << "[ERROR] : Client - "<<this->id<<"Write failed!" << strerror(errno) << endl;
			close(client_fd);
		}

		char buffer[1024] = {0};
		int n = read(client_fd, buffer, 1024);
		if(n > 0)
			buffer[n] = '\0';
		cout<<"[INFO] : Client - "<<this->id<<" Message received in raw from the server: "<<buffer<<endl;
		string response(buffer);
		close(client_fd);
		return response;
	}
};

// FOR THREADS TO ACCEPT CONNECTIONS AND ASSOCIATE THE CONNECTION_FD
struct Task{
    int connection_id;
    struct sockaddr_in address;
    IMessage* request;
    Task() : connection_id(-1), address{}, request{} {}

    Task(int connection_id, struct sockaddr_in address, IMessage* request){
        this->connection_id = connection_id;
        this->address = address;
        this->request = request;
    }
};

// LOCKING MECHANISM
mutex queue_mutex;
mutex secret_mutex; 
condition_variable queue_empty, queue_full;

queue<Task> tasks;
int capacity = 10;
bool kill_switch = false; //TODO: SHUTDOWN NEEDS TO BE CONFIGURED


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
            cout<<"[INFO] : Server - "<<this->id<<" QueryMessage received raw from client: "<<buffer<<endl;
	    	

	    	// Different types of messages
	    	string request(buffer);
	    	// Find the type of message
	    	int i;
	    	for(i=0;i<request.size();i++){
	    		if(request[i] == '#'){
	    			break;
	    		}
	    	}

	    	string type = "";
	    	for(int j=0;j<i;j++){
	    		type += request[j];
	    	}

	    	cout<<"[INFO] : Message type -> "<<type;

	    	IMessage* msg;
	    	if(type=="QUERY"){
				msg = QueryMessage::deserialize(request);

	    	}else if(type == "BUY"){
				msg = BuyMessage::deserialize(request);
	    	}

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
		IMessage* request = task.request;
		cout<<"[INFO] : Server - " << id <<" Read request from the client inside server's thread - " <<this_thread::get_id() <<" from the client: "<<request->content<<" type - "<<request->type<<endl;
		
		const string successMessage = "Success";
		const string errorMessage = "Not found";
		if(request->type == "QUERY"){
			// DECREMENTING HOP COUNT
			request->hop_count  -= 1;


			// IF THE CURRET SERVER HAS THE QueryMessage, THEN SET CONTENT TO SUCCESS AND SEND IT BACK
			bool isAvailable = false;
			{
				lock_guard<mutex> lock(secret_mutex);
				if(request->content == config.SECRET){
					isAvailable = true;
				}
			}
			if(isAvailable){
				QueryMessage msg("QUERY_RESPONSE", successMessage, request->hop_count, request->source_ip, config.SERVER_IP, config.SERVER_PORT_NUMBER);
				string response = msg.serialize();
				write(connection_id, response.c_str(), response.size());

			// ELSE CHECK THE QueryMessages HOP COUNT, IF THE HOP_COUNT = 0, SET NOT FOUND AND RETURN, ELSE CREATE A NEW CLIENT 
			// AND SEND A REQUEST TO THE NEIGHBOR AND FORWARD THE RESPONSE BACK TO THE CLIENT MAKING THE REQUEST.	
			}else{
				if(request->hop_count != 0){
					Client client(config.NEIGHBOR_PORT_NUMBER, config.NEIGHBOR_SERVER_IP, id);
					client.start_connection();
					string response = client.send_request(request);
					write(connection_id, response.c_str(), response.size());
				}else{
					QueryMessage msg("QUERY_RESPONSE", errorMessage, request->hop_count, request->source_ip, config.SERVER_IP, config.SERVER_PORT_NUMBER);
					string response = msg.serialize();
					write(connection_id, response.c_str(), response.size());				
				}	
			}

		}else if(request->type == "BUY"){
			//check if the server is still selling it, if yes then 
				// reduce qty by locking it
					// if qty is 0 and reduction then modify config.secret 
					// to a random element and qty to a random number.
			// else return not selling
			bool isAvailable = false;
			{
				lock_guard<mutex> lock(secret_mutex);
				if(request->content == config.SECRET){
					isAvailable = true;
				}
				config.QTY -= 1;
				if(config.QTY == 0){
					int rand_idx = rand() % (ITEMS.size() - 1);
					config.SECRET = ITEMS[rand_idx];
					config.QTY = rand() % 10;
					cout<<"[INFO] : Server "<<id<<" item changed to "<<config.SECRET<<" with quantity "<<config.QTY<<endl;
				}
			}
			BuyMessage msg;
			if(isAvailable){
				BuyMessage res("BUY_RESPONSE", successMessage, request->source_ip, config.SERVER_IP, config.SERVER_PORT_NUMBER);
				msg = res;	
			}else{
				BuyMessage res("BUY_RESPONSE", errorMessage, request->source_ip, config.SERVER_IP, config.SERVER_PORT_NUMBER);
				msg = res;
			}
			string response = msg.serialize();
			write(connection_id, response.c_str(), response.size());				

		}else{
			cout<<"[ERROR] : Invalid request type!"<<endl;
			string errorMessage = "Invalid Request type!";
			write(connection_id, errorMessage.c_str(), errorMessage.size());				

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
		while(true){
			string probe = config.QUERY_STRING;
			QueryMessage query_message("QUERY", probe, config.HOP_COUNT, config.SERVER_IP, "", -1);

			Client client(config.NEIGHBOR_PORT_NUMBER, config.NEIGHBOR_SERVER_IP, ID);
			client.start_connection();
			cout<<"[INFO] : Client "<<ID<<" has started!"<<endl;

			cout<<"[INFO] : Client "<<ID<<" sending request and awaiting response!"<<endl;
			string response = client.send_request(&query_message);

			cout<<"[INFO] : Client "<<ID<<" Response received! "<<response<<endl;
			IMessage* query_response = QueryMessage::deserialize(response);

			if(query_response->type == "QUERY_RESPONSE" && query_response->content == "Success"){
				cout<<"[INFO] : Client "<<ID<<" Probe successful, next steps, initiate a direct connection to the node, requires the IP of the node"<<endl;
				// initiate a new connection to the destination ip and log the response to the console.
				Client retriever(query_response->destination_port_number, query_response->destination_ip, ID);
				retriever.start_connection();
				cout<<"[INFO] : Client "<<ID<<" Requesting server "<<query_response->destination_ip<<" for item - "<<probe<<endl;
				// initiate a buying message
				BuyMessage buy_message("BUY", config.QUERY_STRING, config.SERVER_IP, query_response->destination_ip, query_response->destination_port_number);
				string buy_res = retriever.send_request(&buy_message);
				IMessage* buy_response = BuyMessage::deserialize(buy_res);
				if(buy_response->type == "BUY_RESPONSE" && buy_response->content == "Success")
					cout<<"[INFO] : Client "<<ID<<" bought "<<config.QUERY_STRING<<" successfully!"<<endl;
				else{
					cout<<"[INFO] : Client "<<ID<<" Probe failed, no such string exists in the system"<<endl;

				}
			}else{
				cout<<"[INFO] : Client "<<ID<<" Probe failed, no such string exists in the system"<<endl;
			}

			int random_idx = rand() % (ITEMS.size() - 1);
			config.QUERY_STRING =  ITEMS[random_idx];
			cout<<"[INFO] : Client "<<ID<<" Cycle completed starting new cycle \n \n \n"<<endl;

			this_thread::sleep_for(chrono::seconds(10));
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

	srand(time(NULL));

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
	config.QTY = 5; // defaults to 5
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
