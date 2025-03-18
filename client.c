#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define server_ip "127.0.0.1"
#define port 8080
#define buffer_size 1024

int sock;
volatile sig_atomic_t client_connection = 1;
pthread_t recv_msg_thread;

//function to disconnect from server
void disconnect(int sig) {
    printf("\nDisconnecting from server....\n");
    client_connection = 0;
    close(sock);
    exit(0);
}

// Function to receive messages
void *receive_messages(void *arg) {
    char buffer[buffer_size];
    while(1) {
        memset(buffer, 0, buffer_size);
        int msgval = recv(sock, buffer, buffer_size - 1, 0);
        if (msgval <= 0) {
            printf("server down.\n");
            client_connection = 0;
            pthread_cancel(recv_msg_thread);
            close(sock);
            break;
        }
        printf("New Message:\n%s\n", buffer);
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[buffer_size];
    
    //create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }
    
    //server address INFO
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    //Converting IP string into binary form
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    //connecting to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed.");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server.\n");
    
    pthread_create(&recv_msg_thread, NULL, receive_messages, NULL);
    pthread_detach(&recv_msg_thread);
    
    signal(SIGINT, disconnect);

    while(client_connection) {
        memset(buffer, 0, buffer_size);
        fgets(buffer, buffer_size, stdin);
        
        // replacing newline character with NULL
        buffer[strcspn(buffer, "\n")] = '\0'; 
        
        if(send(sock, buffer, strlen(buffer), 0) < 0) {
            printf("Failed to send the message.");
        }
    }

    pthread_cancel(recv_msg_thread);
    close(sock);
    return 0;
}