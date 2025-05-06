// server.c - Chat server implementation
#include "common.h"
#include <signal.h>

#define MAX_CLIENTS 10

typedef struct {
    int socket_fd;
    struct sockaddr_in address;
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    int client_count;
    int server_fd;
    int running;
} Server;

Server server;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        server.running = 0;
    }
}

void broadcast_message(const char *message, int sender_socket) {
    for (int i = 0; i < server.client_count; i++) {
        if (server.clients[i].socket_fd != sender_socket) {
            send(server.clients[i].socket_fd, message, strlen(message), 0);
        }
    }
}

void remove_client(int index) {
    close_socket(server.clients[index].socket_fd);

    // Shift clients to fill the gap
    for (int i = index; i < server.client_count - 1; i++) {
        server.clients[i] = server.clients[i + 1];
    }

    server.client_count--;
}

void* handle_client(void *arg) {
    int client_socket = *((int*)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while (server.running) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Received: %s\n", buffer);
            broadcast_message(buffer, client_socket);
        } else if (bytes_read == 0) {
            // Client disconnected
            printf("Client disconnected\n");

            // Find and remove client
            for (int i = 0; i < server.client_count; i++) {
                if (server.clients[i].socket_fd == client_socket) {
                    remove_client(i);
                    break;
                }
            }
            break;
        } else {
            if (server.running) {
                perror("recv failed");

                // Find and remove client
                for (int i = 0; i < server.client_count; i++) {
                    if (server.clients[i].socket_fd == client_socket) {
                        remove_client(i);
                        break;
                    }
                }
                break;
            }
        }
    }

    return NULL;
}

int main() {
    // Initialize server
    memset(&server, 0, sizeof(Server));
    server.running = 1;

    // Set up signal handler
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create server socket
    server.server_fd = create_server_socket();
    printf("Server started on port %d\n", PORT);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Main server loop
    while (server.running) {
        // Accept new connection
        int new_socket = accept(server.server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (new_socket < 0) {
            if (server.running) {
                perror("Accept failed");
            }
            continue;
        }

        if (server.client_count >= MAX_CLIENTS) {
            const char *msg = "*** Server is full, try again later ***";
            send(new_socket, msg, strlen(msg), 0);
            close_socket(new_socket);
            continue;
        }

        // Add client to list
        Client *client = &server.clients[server.client_count];
        client->socket_fd = new_socket;
        client->address = client_addr;
        server.client_count++;

        printf("New client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Create thread to handle client
        pthread_t thread_id;
        int *client_sock = malloc(sizeof(int));
        *client_sock = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, client_sock) != 0) {
            perror("Thread creation failed");
            free(client_sock);
            server.client_count--;
            close_socket(new_socket);
            continue;
        }

        // Detach thread
        pthread_detach(thread_id);
    }

    // Clean up
    printf("\nShutting down server...\n");

    // Close all client connections
    for (int i = 0; i < server.client_count; i++) {
        close_socket(server.clients[i].socket_fd);
    }

    // Close server socket
    close_socket(server.server_fd);

    return EXIT_SUCCESS;
}
