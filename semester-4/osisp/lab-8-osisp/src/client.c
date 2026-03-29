#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 65536
#define RESPONSE_END "\n<END_OF_RESPONSE>\n"

typedef struct ServerResponse {
    char raw[BUFFER_SIZE];
    char status[16];
    char pwd[PATH_MAX];
    char* payload;
} ServerResponse;

static volatile sig_atomic_t client_stop = 0;

static void handle_signal(int sig);
static int send_all(int fd, const char* buffer, size_t length);
static int send_command(int fd, const char* command);
static int connect_to_server(const char* host, const char* port);
static int parse_response(ServerResponse* response);
static int read_server_response(int fd, ServerResponse* response);
static void build_prompt(const char* pwd, char* buffer, size_t buffer_size);
static void print_payload(const char* payload);
static int execute_command(int fd, const char* command, char* prompt, size_t prompt_size);
static int process_file_commands(int fd, const char* filename, char* prompt, size_t prompt_size);
static void interactive_loop(int fd, char* prompt, size_t prompt_size);

int main(int argc, char* argv[])
{
    const char* host = NULL;
    const char* port = "12345";

    if (argc == 2) {
        host = argv[1];
    } else if (argc == 3) {
        host = argv[1];
        port = argv[2];
    } else {
        fprintf(stderr, "Usage: %s <host> [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    int server_socket = connect_to_server(host, port);
    if (server_socket < 0) {
        return EXIT_FAILURE;
    }

    printf("Connected to %s:%s\n", host, port);

    ServerResponse welcome;
    if (read_server_response(server_socket, &welcome) != 0) {
        perror("Failed to read welcome message");
        close(server_socket);
        return EXIT_FAILURE;
    }

    char prompt[PATH_MAX + 4];
    build_prompt(welcome.pwd, prompt, sizeof(prompt));
    print_payload(welcome.payload);

    interactive_loop(server_socket, prompt, sizeof(prompt));

    close(server_socket);
    return 0;
}

static void handle_signal(int sig)
{
    (void)sig;
    client_stop = 1;
}

static int send_all(int fd, const char* buffer, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t written = write(fd, buffer + sent, length - sent);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)written;
    }
    return 0;
}

static int send_command(int fd, const char* command)
{
    char request[BUFFER_SIZE];
    int request_length = snprintf(request, sizeof(request), "%s\n", command);
    if (request_length < 0 || (size_t)request_length >= sizeof(request)) {
        fprintf(stderr, "Command is too long.\n");
        errno = EMSGSIZE;
        return -1;
    }

    return send_all(fd, request, (size_t)request_length);
}

static int connect_to_server(const char* host, const char* port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = NULL;
    int gai_status = getaddrinfo(host, port, &hints, &result);
    if (gai_status != 0) {
        fprintf(stderr, "Failed to resolve %s:%s: %s\n", host, port, gai_strerror(gai_status));
        return -1;
    }

    int server_socket = -1;
    for (struct addrinfo* item = result; item != NULL; item = item->ai_next) {
        server_socket = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (server_socket < 0) {
            continue;
        }
        if (connect(server_socket, item->ai_addr, item->ai_addrlen) == 0) {
            break;
        }
        close(server_socket);
        server_socket = -1;
    }

    freeaddrinfo(result);

    if (server_socket < 0) {
        perror("Failed to connect to server");
    }

    return server_socket;
}

static int parse_response(ServerResponse* response)
{
    char* payload = strstr(response->raw, "\n\n");
    if (payload == NULL) {
        errno = EPROTO;
        return -1;
    }

    *payload = '\0';
    payload += 2;

    char* second_line = strchr(response->raw, '\n');
    if (second_line == NULL) {
        errno = EPROTO;
        return -1;
    }
    *second_line = '\0';
    ++second_line;

    if (strncmp(response->raw, "STATUS ", 7) != 0 || strncmp(second_line, "PWD ", 4) != 0) {
        errno = EPROTO;
        return -1;
    }

    snprintf(response->status, sizeof(response->status), "%s", response->raw + 7);
    snprintf(response->pwd, sizeof(response->pwd), "%s", second_line + 4);
    response->payload = payload;
    return 0;
}

static int read_server_response(int fd, ServerResponse* response)
{
    size_t total = 0;
    response->raw[0] = '\0';

    while (total + 1 < sizeof(response->raw)) {
        ssize_t received = read(fd, response->raw + total, sizeof(response->raw) - total - 1);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (received == 0) {
            errno = ECONNRESET;
            return -1;
        }

        total += (size_t)received;
        response->raw[total] = '\0';

        char* end_marker = strstr(response->raw, RESPONSE_END);
        if (end_marker != NULL) {
            *end_marker = '\0';
            return parse_response(response);
        }
    }

    errno = EMSGSIZE;
    return -1;
}

static void build_prompt(const char* pwd, char* buffer, size_t buffer_size)
{
    if (pwd[0] == '\0' || strcmp(pwd, "/") == 0) {
        snprintf(buffer, buffer_size, "> ");
    } else {
        snprintf(buffer, buffer_size, "%s> ", pwd);
    }
}

static void print_payload(const char* payload)
{
    if (payload == NULL || payload[0] == '\0') {
        return;
    }

    printf("%s", payload);
    if (payload[strlen(payload) - 1] != '\n') {
        putchar('\n');
    }
}

static int execute_command(int fd, const char* command, char* prompt, size_t prompt_size)
{
    if (send_command(fd, command) != 0) {
        perror("Failed to send command");
        return -1;
    }

    ServerResponse response;
    if (read_server_response(fd, &response) != 0) {
        perror("Failed to read server response");
        return -1;
    }

    build_prompt(response.pwd, prompt, prompt_size);
    print_payload(response.payload);

    if (strcmp(response.status, "BYE") == 0) {
        return 1;
    }

    return 0;
}

static int process_file_commands(int fd, const char* filename, char* prompt, size_t prompt_size)
{
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open command file");
        return -1;
    }

    char command[BUFFER_SIZE];
    while (!client_stop && fgets(command, sizeof(command), file) != NULL) {
        command[strcspn(command, "\r\n")] = '\0';
        if (command[0] == '\0') {
            continue;
        }

        if (command[0] == '@') {
            int nested = process_file_commands(fd, command + 1, prompt, prompt_size);
            if (nested != 0) {
                fclose(file);
                return nested;
            }
            continue;
        }

        int status = execute_command(fd, command, prompt, prompt_size);
        if (status != 0) {
            fclose(file);
            return status;
        }
    }

    fclose(file);
    return 0;
}

static void interactive_loop(int fd, char* prompt, size_t prompt_size)
{
    char command[BUFFER_SIZE];
    while (!client_stop) {
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            putchar('\n');
            break;
        }

        command[strcspn(command, "\r\n")] = '\0';
        if (command[0] == '\0') {
            continue;
        }

        int status;
        if (command[0] == '@') {
            status = process_file_commands(fd, command + 1, prompt, prompt_size);
        } else {
            status = execute_command(fd, command, prompt, prompt_size);
        }

        if (status != 0) {
            break;
        }
    }
}
