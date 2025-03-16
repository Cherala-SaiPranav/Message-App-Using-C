#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define server_ip "127.0.0.1"
#define port 8080
#define buffer_size 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[buffer_size] = {0};
    char message[buffer_size];
    int connection = 1;
    int process;

    //create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    //server address INFO
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    //Converting IP string into binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Converting IP string into binary form failed.\n");
        exit(EXIT_FAILURE);
    }

    //connecting to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed.");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server.\n");


    printf("%s\n", buffer);
    while(connection){
        printf("Enter '0' to disconnect from server.\nEnter '1' to send a message to server\nEnter '2' to check for new messages.\n");
        scanf("%d", &process);
        getchar();
        if (process == 0) {
            connection = 0;
            //closing socket
            close(sock);
            break;
        } else if (process == 1) {

            printf("Enter the message.\n");
            fgets(message, sizeof(message), stdin);
            // Remove the trailing newline
            message[strcspn(message, "\n")] = '\0';
            //sending data to server
            send(sock, message, strlen(message) + 1, 0);
            printf("Message sent to server.\n");

        } else if(process == 2) {

        //Receicing data form server
        int Recdata = read(sock, buffer, buffer_size);
        if (Recdata > 0) {
            //Null terminate the received data
            buffer[Recdata] = '\0';
            printf("Received from server: %s\n", buffer);
        }
        }else if(process < 0 || process > 2){
            printf("Wrong Input.\nTry again.\n");
        }

    }

    return 0;
}