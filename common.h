// common.h - Shared definitions for the chat application
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <pthread.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_MESSAGES 100
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_USERNAME_LEN 32

typedef struct {
    char content[BUFFER_SIZE];
    int is_self;
} Message;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    int socket_fd;
    Message messages[MAX_MESSAGES];
    int message_count;
    char input_text[BUFFER_SIZE];
    int input_pos;
    char username[MAX_USERNAME_LEN];
    pthread_t recv_thread;
    int running;
} ChatApp;

// Socket utility functions
int create_socket(void);
int connect_to_server(int socket_fd, const char *server_ip);
int create_server_socket(void);
void close_socket(int socket_fd);

// Message handling
void add_message(ChatApp *app, const char *content, int is_self);
void send_message(ChatApp *app, const char *message);

// SDL UI functions
int init_sdl(ChatApp *app);
void cleanup_sdl(ChatApp *app);
void render_messages(ChatApp *app);
void render_input(ChatApp *app);
void handle_text_input(ChatApp *app, const char *text);
void handle_key(ChatApp *app, SDL_Keycode key);

#endif // COMMON_H
