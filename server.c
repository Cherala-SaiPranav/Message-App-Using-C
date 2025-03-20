#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CLIENTS 5
#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char unique_name[12];
    int active;
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t server_running = 1;
int server_fd;

// Function to send private message
void private_message(char *message, char *recipient_name, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    int found = 0;

    printf("\n[DEBUG] Searching for recipient: %s\n", recipient_name);
    printf("[DEBUG] Active clients:\n");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            printf("Client %d: %s\n", i, clients[i].unique_name);
            printf("[DEBUG] strcmp('%s', '%s') = %d\n", clients[i].unique_name, recipient_name, strcmp(clients[i].unique_name, recipient_name));


            if (strcmp(clients[i].unique_name, recipient_name) == 0) {
                send(clients[i].socket, message, strlen(message), 0);
                found = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!found) {
        char error_message[BUFFER_SIZE];
        snprintf(error_message, BUFFER_SIZE, "User %s not found.", recipient_name);
        send(sender_socket, error_message, strlen(error_message), 0);
    }
}

// Function to broadcast messages to all clients except sender
void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket != sender_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Function to handle client communication
void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Receive client’s name
    bytes_read = recv(client->socket, client->unique_name, sizeof(client->unique_name) - 1, 0);
    if (bytes_read <= 0) {
        close(client->socket);
        client->active = 0;
        return NULL;
    }
    client->unique_name[bytes_read] = '\0';

    // Remove any trailing newline or spaces
    size_t len = strlen(client->unique_name);
    while (len > 0 && (client->unique_name[len - 1] == '\n' || client->unique_name[len - 1] == ' ')) {
        client->unique_name[len - 1] = '\0';
        len--;
}

    printf("Unique name %s is stored in array.", client->unique_name);

    printf("%s connected to the server successfully.\n", client->unique_name);

    while ((bytes_read = recv(client->socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';

        // Private message handling
        if (buffer[0] == '@') {
            char recipient[12], msg[BUFFER_SIZE];
            char *space_position = strchr(buffer, ' ');

            if (space_position) {
                *space_position = '\0';
                strcpy(recipient, buffer + 1);

                // Trim trailing newline or spaces
                size_t len = strlen(recipient);
                if (len > 0 && (recipient[len - 1] == '\n' || recipient[len - 1] == ' ')) {
                recipient[len - 1] = '\0';
                }

                strcpy(msg, space_position + 1);

                char formatted_message[BUFFER_SIZE];
                snprintf(formatted_message, BUFFER_SIZE, "@%s (Private): %s", client->unique_name, msg);
                private_message(formatted_message, recipient, client->socket);
            } else {
                char error_message[] = "Invalid private message format. Use: @recipient_name Message";
                send(client->socket, error_message, strlen(error_message), 0);
            }
        } else {
            // Broadcast message
            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, BUFFER_SIZE, "%s: %s", client->unique_name, buffer);
            broadcast_message(formatted_message, client->socket);
        }
    }

    // Mark client as inactive on disconnect
    pthread_mutex_lock(&clients_mutex);
    client->active = 0;
    pthread_mutex_unlock(&clients_mutex);

    close(client->socket);
    return NULL;
}

// Function to handle server shutdown
void server_shutdown(int sig) {
    printf("\nShutting down server...\n");
    server_running = 0;
    close(server_fd);
}

int main() {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // Initialize client slots as inactive
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    signal(SIGINT, server_shutdown);

    while (server_running) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            if (!server_running) break;
            printf("Accept failed.\n");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int assigned = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                clients[i].socket = new_socket;
                clients[i].addr = address;
                clients[i].active = 1;

                pthread_t client_thread;
                pthread_create(&client_thread, NULL, handle_client, (void *)&clients[i]);
                pthread_detach(client_thread);
                assigned = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!assigned) {
            printf("Max clients reached. Connection rejected.\n");
            close(new_socket);
        }
    }

    printf("Closing all client connections...\n");
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            close(clients[i].socket);
            clients[i].active = 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(server_fd);
    return 0;
}
