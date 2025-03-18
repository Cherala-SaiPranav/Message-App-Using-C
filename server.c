#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define max_clients 5
#define port 8080
#define buffer_size 1024

typedef struct {
    int socket;
    struct sockaddr_in addr;
}Client;

Client clients[max_clients];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t server_running = 1;
int server_fd;

//funtion to shutdown server
void server_shutdown(int sig) {
    printf("\nShutting down server....\n");
    server_running = 0;
    close(server_fd);
}

// broadcasting message to all clients except the sender
void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    char Modified_msg[buffer_size] = {0};

    //Adding sender's info to message
    for(int i = 0; i < buffer_size; i++) {
        if (clients[i].socket == sender_socket) {
            snprintf(Modified_msg, buffer_size, "\tFrom: %s:%d\n\tMessage: %s", inet_ntoa(clients[i].addr.sin_addr), ntohs(clients[i].addr.sin_port), message);
            break;
        }
    }

    //sending message
    for(int i = 0; i < max_clients; i++) {
        if(clients[i].socket != sender_socket && clients[i].socket != 0) {
            send(clients[i].socket, Modified_msg, strlen(Modified_msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//function to handle communication

//the function and arg is a pointer because pthread_create expects pointers as it's arguments
void *handle_client(void *arg) { 

     // typrcasting the argument back into Client struct type
    Client *client = (Client *)arg;

    char buffer[buffer_size];
    int bytes_read;

    // receiving message from client
    while((bytes_read = recv(client->socket, buffer, buffer_size - 1, 0)) > 0) {
        // adding the null character at last
        buffer[bytes_read] = '\0';
        broadcast_message(buffer, client->socket);
    }

    // removing the client after being disconnected
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < max_clients; i++) {
        if (clients[i].socket == client->socket) {
            clients[i].socket = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(client->socket);
    free(client);
    return NULL;
}
    
int main() {
    int new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[buffer_size];
    
    //create a socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket Creation Failed.");
        exit(EXIT_FAILURE);
    }

    //Server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //Bind socket to IP and Port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Binding Failed");
        exit(EXIT_FAILURE);
    }

    // Listening for connections
    if(listen(server_fd, max_clients) < 0) {
        perror("Failed Listen");
        exit(EXIT_FAILURE);
    }

    printf("server listening on port %d\n", port);

    //When SIGINT(ctrl+c) happens run server_shutdown function
    signal(SIGINT, server_shutdown);

    while(server_running) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            if (!server_running) break;
            printf("Accept failed.");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int client_count = 0;
        for (int i = 0; i < max_clients; i++) {
            if (clients[i].socket != 0) {
                client_count++;
                if (client_count >= max_clients) {
                    printf("Max clients capacity reached. rejecting new connections...\n");
                    close(new_socket);
                    continue;
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        // using malloc to dynamically allocate memory to clients
        Client *new_client = (Client *)malloc(sizeof(Client));
        if(!new_client) {
            perror("Memory allocation failed.\n");
            close(new_socket);
            continue;
        }
        new_client->socket = new_socket;
        new_client->addr = address;

        //storing client info
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < max_clients; i++) {
            if (clients[i].socket == 0) {
                clients[i] = *new_client;
                break;
            }
        }

        pthread_mutex_unlock(&clients_mutex);

        //creating a seperate thread for each client
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, (void *) new_client);
        pthread_detach(&client_thread);
    }
    
    //Closing all active clients before shutdown
    printf("Closing all client's connection....\n");
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < max_clients; i++) {
        if(clients[i].socket != 0) {
            close(clients[i].socket);
            clients[i].socket = 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(server_fd);
    return 0;
}