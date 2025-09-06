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

using namespace std;

/* 
 * A server - socket -> bind -> listen -> accept -> read/write -> close
 *
 * A client - socket -> connect ->read/write -> close
 *
 */

#define NUM_THREADS 4

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
bool kill_switch = false;

condition_variable queue_empty, queue_full;

string secret = "thor";

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
		// lock the queue and get an element - condition variable required 
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
			const char* errorMessage = "Not found";
			write(connection_id, errorMessage, strlen(errorMessage));
		}
		close(connection_id);
	}

}

int main(){
	int PORT_NUMBER = 8082;
	int CONNECTION_NUMBER = 5;
	
	
	vector<thread> threads;
	for(int i=0;i<NUM_THREADS; i++){
		thread t(executeTask);
		threads.push_back(move(t));
	}
	Server server(PORT_NUMBER, CONNECTION_NUMBER);
	server.start();
	
	for(int i=0;i<NUM_THREADS;i++){
		threads[i].join();
	}
    return 0;
}
