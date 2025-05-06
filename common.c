// common.c - Implementation of shared functions
#include "common.h"

// Socket utility functions
int create_socket(void) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sock_fd;
}

int connect_to_server(int socket_fd, const char *server_ip) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return -1;
    }

    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    return 0;
}

int create_server_socket(void) {
    int server_fd = create_socket();
    struct sockaddr_in address;
    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    return server_fd;
}

void close_socket(int socket_fd) {
    close(socket_fd);
}

// Message handling
void add_message(ChatApp *app, const char *content, int is_self) {
    if (app->message_count >= MAX_MESSAGES) {
        // Shift messages to make room
        memmove(app->messages, app->messages + 1, sizeof(Message) * (MAX_MESSAGES - 1));
        app->message_count--;
    }

    Message *msg = &app->messages[app->message_count];
    strncpy(msg->content, content, BUFFER_SIZE - 1);
    msg->content[BUFFER_SIZE - 1] = '\0';
    msg->is_self = is_self;
    app->message_count++;
}

void send_message(ChatApp *app, const char *message) {
    char formatted[BUFFER_SIZE];
    snprintf(formatted, BUFFER_SIZE, "%s: %s", app->username, message);
    send(app->socket_fd, formatted, strlen(formatted), 0);
    add_message(app, formatted, 1);
}

// SDL UI functions
int init_sdl(ChatApp *app) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF Error: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    app->window = SDL_CreateWindow("Chat Application",
                                  SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!app->window) {
        fprintf(stderr, "Window could not be created! SDL Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
    if (!app->renderer) {
        fprintf(stderr, "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(app->window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load font - fallback to system fonts if not found
    app->font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!app->font) {
        app->font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", 16);
    }
    if (!app->font) {
        fprintf(stderr, "Failed to load font! TTF Error: %s\n", TTF_GetError());
        // Continue anyway with a warning
    }

    // Start with empty input
    app->input_text[0] = '\0';
    app->input_pos = 0;
    app->message_count = 0;
    app->running = 1;

    return 0;
}

void cleanup_sdl(ChatApp *app) {
    if (app->font) {
        TTF_CloseFont(app->font);
    }
    if (app->renderer) {
        SDL_DestroyRenderer(app->renderer);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }
    TTF_Quit();
    SDL_Quit();
}

void render_messages(ChatApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 240, 240, 240, 255);
    SDL_Rect messages_area = {10, 10, WINDOW_WIDTH - 20, WINDOW_HEIGHT - 60};
    SDL_RenderFillRect(app->renderer, &messages_area);

    if (!app->font) return;  // Skip text rendering if font failed to load

    SDL_Color self_color = {0, 0, 200, 255};  // Blue for own messages
    SDL_Color other_color = {200, 0, 0, 255}; // Red for others' messages

    int y_pos = WINDOW_HEIGHT - 80;

    // Display messages from newest (bottom) to oldest (top)
    for (int i = app->message_count - 1; i >= 0; i--) {
        SDL_Surface* text_surface = TTF_RenderText_Blended(
            app->font,
            app->messages[i].content,
            app->messages[i].is_self ? self_color : other_color
        );

        if (text_surface) {
            SDL_Texture* text_texture = SDL_CreateTextureFromSurface(app->renderer, text_surface);
            if (text_texture) {
                SDL_Rect text_rect = {20, y_pos - text_surface->h, text_surface->w, text_surface->h};
                SDL_RenderCopy(app->renderer, text_texture, NULL, &text_rect);
                SDL_DestroyTexture(text_texture);
                y_pos -= text_surface->h + 5;
            }
            SDL_FreeSurface(text_surface);
        }

        // Stop if we've reached the top of the messages area
        if (y_pos < 20) break;
    }
}

void render_input(ChatApp *app) {
    // Draw input box
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
    SDL_Rect input_area = {10, WINDOW_HEIGHT - 40, WINDOW_WIDTH - 20, 30};
    SDL_RenderFillRect(app->renderer, &input_area);

    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(app->renderer, &input_area);

    if (!app->font) return;  // Skip text rendering if font failed to load

    // Render input text
    SDL_Color text_color = {0, 0, 0, 255};
    SDL_Surface* text_surface = TTF_RenderText_Blended(app->font, app->input_text, text_color);

    if (text_surface) {
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(app->renderer, text_surface);
        if (text_texture) {
            SDL_Rect text_rect = {15, WINDOW_HEIGHT - 35, text_surface->w, text_surface->h};
            SDL_RenderCopy(app->renderer, text_texture, NULL, &text_rect);
            SDL_DestroyTexture(text_texture);
        }
        SDL_FreeSurface(text_surface);
    }
}

void handle_text_input(ChatApp *app, const char *text) {
    size_t text_len = strlen(text);
    size_t input_len = strlen(app->input_text);

    if (input_len + text_len < BUFFER_SIZE - 1) {
        strncat(app->input_text, text, BUFFER_SIZE - input_len - 1);
        app->input_pos += text_len;
    }
}

void handle_key(ChatApp *app, SDL_Keycode key) {
    size_t len = strlen(app->input_text);

    switch (key) {
        case SDLK_BACKSPACE:
            if (len > 0) {
                app->input_text[len - 1] = '\0';
                app->input_pos--;
            }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (len > 0) {
                send_message(app, app->input_text);
                app->input_text[0] = '\0';
                app->input_pos = 0;
            }
            break;
        case SDLK_ESCAPE:
            app->running = 0;
            break;
    }
}
