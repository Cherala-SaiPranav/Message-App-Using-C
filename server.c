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
    char unique_name[50];
}Client;

Client *clients[max_clients];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t server_running = 1;
int server_fd;

//function for private messaging
void private_message(char *message, char *recipient_name, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    int found = 0;

    for(int i = 0; i < max_clients; i++) {
        if(clients[i] && strcmp(clients[i]->unique_name, recipient_name) == 0) {
            send(clients[i]->socket, message, strlen(message), 0);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if(!found) {
        char error_message[buffer_size];
        snprintf(error_message, buffer_size, "User %s not found.", recipient_name);
        send(sender_socket, error_message, strlen(error_message), 0);
    }
}

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
    for(int i = 0; i < max_clients; i++) {
        printf("%s\n", clients[i]->unique_name);
        if (clients[i]->socket == sender_socket) {
            snprintf(Modified_msg, buffer_size, "\tFrom: %s:%d\n\tMessage: %s", inet_ntoa(clients[i]->addr.sin_addr), ntohs(clients[i]->addr.sin_port), message);
            break;
        }
    }

    //sending message
    for(int i = 0; i < max_clients; i++) {
        if(clients[i]->socket != sender_socket && clients[i]) {
            send(clients[i]->socket, Modified_msg, strlen(Modified_msg), 0);
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

    //receive client's name
    bytes_read = recv(client->socket, client->unique_name, sizeof(client->unique_name) - 1, 0);
    if (bytes_read <= 0) {
        close(client->socket);
        free(client);
        return NULL;
    }
    client->unique_name[bytes_read] = '\0';

    //storing unique name in clients shared array
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0;i < max_clients; i++) {
        if(!clients[i]) {
            clients[i] = client;
            break;
        }
    }
    printf("%s connected to server successfully.\n", client->unique_name);
    pthread_mutex_unlock(&clients_mutex);

    // receiving message from client
    while((bytes_read = recv(client->socket, buffer, buffer_size - 1, 0)) > 0) {
        // adding the null character at last
        buffer[bytes_read] = '\0';

        // private Message by extracting recipient name and message
        if(buffer[0] == '@') {
            char recipient[50], msg[buffer_size];
            char *space_position = strchr(buffer, ' ');
            if (space_position) {

                // splitting recipient and message
                *space_position = '\0';
                strcpy(recipient, buffer + 1);
                printf("Sending message to %s", recipient);
                strcpy(msg, space_position + 1);
                printf("the message is %s", msg);
            
                char formatted_message[buffer_size];
                snprintf(formatted_message, buffer_size, "@%s (Private Message): %s", client->unique_name, msg);
                private_message(formatted_message, recipient, client->socket);
            } else {
                char error_message[] = "Invalid private message format. Correct format: @recipient-username Message";
                send(client->socket, error_message, strlen(error_message), 0);
            }
        } else {
            // broadcaasting message to all clients
            char formatted_message[buffer_size];
            snprintf(formatted_message, buffer_size, "%s: %s", client->unique_name, buffer);
            broadcast_message(formatted_message, client->socket);
        }
    }

    // removing the client after being disconnected
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < max_clients; i++) {
        if (clients[i] == client) {
            clients[i] = NULL;
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
            if (clients[i] != NULL) {
                client_count++;
            }
        }

        if (client_count >= max_clients) {
            pthread_mutex_unlock(&clients_mutex);
            printf("Max clients capacity reached. rejecting new connections...\n");
            close(new_socket);
            continue;
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

        // //storing client info
        // pthread_mutex_lock(&clients_mutex);
        // for (int i = 0; i < max_clients; i++) {
        //     if (clients[i].socket == 0) {
        //         clients[i] = *new_client;
        //         break;
        //     }
        // }

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
        if(clients[i]) {
            close(clients[i]->socket);
            clients[i] = NULL;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(server_fd);
    return 0;
}