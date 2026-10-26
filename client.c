
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 512
#define RESPONSE_SIZE 1024

//for the commands, "_" represents a space bar and anything in () is what you input for that section
int main(void)
{
    char request[BUFFER_SIZE];
    char response[RESPONSE_SIZE];
    int client_fd;
    struct sockaddr_in server_address;

    printf("Reservation TCP Client\n");
    printf("Commands:\n");
    printf("  LIST\n");
    printf("  HOLD_trip(1,2,3)_seat(1-24)_firstname_lastname\n");
    printf("  CONFIRM_trip(1,2,3)_seat(1-24)\n");
    printf("  CANCEL_trip(1,2,3)_seat(1-24)\n");
    printf("  WAITLIST_trip(1,2,3)_firstname_lastname\n");
    printf("Type QUIT to exit.\n\n");

    for (;;) {
        size_t total_received = 0;

        printf("Request: ");
        fflush(stdout);

        if (fgets(request, sizeof request, stdin) == NULL) {
            break;
        }

        request[strcspn(request, "\n")] = '\0';

        if (strcmp(request, "QUIT") == 0 ||
            strcmp(request, "quit") == 0) {
            break;
        }

        if (request[0] == '\0') {
            continue;
        }

        client_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (client_fd == -1) {
            perror("Could not create client socket");
            continue;
        }

        memset(&server_address, 0, sizeof server_address);
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(SERVER_PORT);

        if (inet_pton(AF_INET, "127.0.0.1",
                      &server_address.sin_addr) != 1) {
            fprintf(stderr, "Invalid server address\n");
            close(client_fd);
            continue;
        }

        if (connect(client_fd, (struct sockaddr *)&server_address,
                    sizeof server_address) == -1) {
            perror("Could not connect to server");
            close(client_fd);
            continue;
        }

        if (send(client_fd, request, strlen(request), 0) == -1 ||
            send(client_fd, "\n", 1, 0) == -1) {
            perror("Could not send request");
            close(client_fd);
            continue;
        }

        shutdown(client_fd, SHUT_WR);

        printf("\nServer response:\n");

        for (;;) {
            ssize_t received = recv(client_fd, response,
                                    sizeof response - 1, 0);

            if (received == 0) {
                break;
            }

            if (received < 0) {
                perror("Could not receive server response");
                break;
            }

            response[received] = '\0';
            fputs(response, stdout);
            total_received += (size_t)received;
        }

        if (total_received == 0) {
            printf("(No response received)\n");
        }

        printf("\n");
        close(client_fd);
    }

    printf("Client closed.\n");
    return EXIT_SUCCESS;
}
