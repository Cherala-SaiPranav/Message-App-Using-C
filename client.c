#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define server_ip "127.0.0.1"
#define port 8080
#define buffer_size 1024

int sock;

// Function to receive messages
void *receive_messages(void *arg) {
    char buffer[buffer_size];
    while(1) {
        memset(buffer, 0, buffer_size);
        int msgval = recv(sock, buffer, buffer_size - 1, 0);
        if (msgval <= 0) {
            printf("server down.\n");
            exit(0);
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
        exit(EXIT_FAILURE);
    }
    printf("Connected to server.\n");

    pthread_t recv_msg_thread;
    pthread_create(&recv_msg_thread, NULL, receive_messages, NULL);
    pthread_detach(&recv_msg_thread);

    while(1) {
        memset(buffer, 0, buffer_size);
        fgets(buffer, buffer_size, stdin);

        // replacing newline character with NULL
        buffer[strcspn(buffer, "\n")] = '\0'; 
        
        if(strcmp(buffer, "exit") == 0) {
            printf("Disconnecting....\n");
            break;
        }
        send(sock, buffer, strlen(buffer), 0);
    }

    close(sock);
    return 0;
}