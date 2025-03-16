#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define port 8080
#define buffer_size 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[buffer_size] = {0};
    char message[buffer_size];
    int server_status = 1;
    int process;

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
    if(listen(server_fd, 5) < 0) {
        perror("Failed Listen");
        exit(EXIT_FAILURE);
    }

    printf("server listening on port %d\n", port);

    //Accepting client's connection request
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accepting client failed.");
    }

    printf("Client Connection Successfull.\n");


    while(server_status) {

        printf("Enter '0' to stop server.\nEnter '1' to send a message to client\nEnter '2' to check for new messages\n");
        scanf("%d", &process);
        getchar();

        if (process == 0) {

            server_status = 0;

            //Closing sockets
            close(new_socket);
            close(server_fd);

            break;

        } else if (process == 1) {

            printf("Enter the message.\n");
            fgets(message, sizeof(message), stdin);

            // Remove the trailing newline
            message[strcspn(message, "\n")] = '\0';

            //sending message to client
            send(new_socket, message, strlen(message)+ 1, 0);

        }else if (process == 2) {

            //clear buffer before reading
            memset(buffer, 0, buffer_size);
            
            //receive data from client
            int RecData = read(new_socket, buffer, buffer_size - 1);
            
            if (RecData > 0) {

                //Null terminate the received data
                buffer[RecData] = '\0';
                printf("Received from client: %s\n", buffer);
            
            } else if (RecData == 0) {
                
                printf("Client disconnected.\n");

                //Closing sockets
                close(new_socket);
                close(server_fd);
                
                break;
            }
        
        }else if (process < 0 || process > 2){

            printf("Wrong Input.\nTry again.\n");
        }
    }

    return 0;
}