// client.c - Chat client implementation
#include "common.h"

// Thread function to receive messages
void* receive_messages(void *arg) {
    ChatApp *app = (ChatApp*)arg;
    char buffer[BUFFER_SIZE];

    while (app->running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(app->socket_fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            add_message(app, buffer, 0);

            // In nogui mode, print messages directly to terminal
            if (app->nogui) {
                printf("%s\n", buffer);
            }
        } else if (bytes_read == 0) {
            // Connection closed
            add_message(app, "*** Server disconnected ***", 0);
            if (app->nogui) {
                printf("*** Server disconnected ***\n");
            }
            app->running = 0;
            break;
        } else {
            if (app->running) {
                perror("recv failed");
                app->running = 0;
                break;
            }
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <server_ip> <username> [--nogui]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];

    ChatApp app;
    memset(&app, 0, sizeof(ChatApp));

    // Set username
    strncpy(app.username, argv[2], MAX_USERNAME_LEN - 1);
    app.username[MAX_USERNAME_LEN - 1] = '\0';

    // Check for nogui flag
    app.nogui = 0;
    if (argc == 4 && strcmp(argv[3], "--nogui") == 0) {
        app.nogui = 1;
    }

    // Initialize SDL only if not in nogui mode
    if (!app.nogui) {
        if (init_sdl(&app) < 0) {
            fprintf(stderr, "Failed to initialize SDL!\n");
            return EXIT_FAILURE;
        }
    }

    // Create socket and connect to server
    app.socket_fd = create_socket();
    if (connect_to_server(app.socket_fd, server_ip) < 0) {
        fprintf(stderr, "Failed to connect to server!\n");
        cleanup_sdl(&app);
        close_socket(app.socket_fd);
        return EXIT_FAILURE;
    }

    app.running = 1;

    // Start receive thread
    if (pthread_create(&app.recv_thread, NULL, receive_messages, &app) != 0) {
        perror("Failed to create receive thread");
        cleanup_sdl(&app);
        close_socket(app.socket_fd);
        return EXIT_FAILURE;
    }

    // Send join message
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, BUFFER_SIZE, "*** %s has joined the chat ***", app.username);
    send(app.socket_fd, join_msg, strlen(join_msg), 0);

    // Main loop
    if (app.nogui) {
        // Terminal mode
        printf("Chat started. Type your messages and press Enter. Use Ctrl+C to exit.\n");

        while (app.running) {
            char input[BUFFER_SIZE];
            if (fgets(input, BUFFER_SIZE - 1, stdin) != NULL) {
                // Remove newline
                size_t len = strlen(input);
                if (len > 0 && input[len - 1] == '\n') {
                    input[len - 1] = '\0';
                }

                // Send message
                if (strlen(input) > 0) {
                    send_message(&app, input);
                }
            }
        }
    } else {
        // GUI mode
        SDL_Event e;
        SDL_StartTextInput();

        while (app.running) {
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_QUIT:
                        app.running = 0;
                        break;
                    case SDL_TEXTINPUT:
                        handle_text_input(&app, e.text.text);
                        break;
                    case SDL_KEYDOWN:
                        handle_key(&app, e.key.keysym.sym);
                        break;
                }
            }

            // Render
            SDL_SetRenderDrawColor(app.renderer, 200, 200, 200, 255);
            SDL_RenderClear(app.renderer);

            render_messages(&app);
            render_input(&app);

            SDL_RenderPresent(app.renderer);
            SDL_Delay(16); // ~60 FPS
        }

        // Clean up GUI
        SDL_StopTextInput();
    }
    app.running = 0;

    // Send leave message
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, BUFFER_SIZE, "*** %s has left the chat ***", app.username);
    send(app.socket_fd, leave_msg, strlen(leave_msg), 0);

    // Wait for recv thread to terminate
    pthread_join(app.recv_thread, NULL);

    close_socket(app.socket_fd);
    if (!app.nogui) {
        cleanup_sdl(&app);
    }

    return EXIT_SUCCESS;
}
