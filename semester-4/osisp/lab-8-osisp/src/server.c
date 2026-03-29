#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define COMMAND_SIZE 4096
#define RESPONSE_END "\n<END_OF_RESPONSE>\n"
#define MAX_COMPONENTS 256

typedef struct ClientNode {
    int clientfd;
    pthread_t thread;
    struct ClientNode* next;
} ClientNode;

typedef struct ClientArgs {
    int clientfd;
    int client_port;
    char client_ip[INET_ADDRSTRLEN];
    const char* root_dir;
} ClientArgs;

static volatile sig_atomic_t stop_server = 0;
static int listen_fd = -1;
static char* server_info = NULL;
static ClientNode* clients = NULL;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static void handle_signal(int sig);
static void format_timestamp(char* buffer, size_t buffer_size);
static void log_server_event(const char* fmt, ...);
static int send_all(int fd, const char* buffer, size_t length);
static int send_response(int fd, const char* status, const char* current_dir,
    const char* root_dir, const char* payload);
static void add_client(int clientfd, pthread_t thread);
static void remove_client(pthread_t thread);
static void close_all_clients(void);
static char* duplicate_string(const char* text);
static char* read_server_info(void);
static int path_within_root(const char* path, const char* root_dir);
static void get_root_relative(const char* path, const char* root_dir,
    char* buffer, size_t buffer_size);
static void get_prompt_path(const char* current_dir, const char* root_dir,
    char* buffer, size_t buffer_size);
static int split_path_components(const char* path,
    char components[MAX_COMPONENTS][NAME_MAX + 1]);
static int build_relative_path(const char* from_dir, const char* to_path,
    char* buffer, size_t buffer_size);
static int normalize_target_path(const char* root_dir, const char* base_dir,
    const char* raw_target, char* buffer, size_t buffer_size);
static void get_target_display_path(const char* current_dir, const char* root_dir,
    const char* target_path, char* buffer, size_t buffer_size);
static int append_part(char* buffer, size_t buffer_size, const char* part);
static int append_text(char** buffer, size_t* length, size_t* capacity, const char* text);
static int append_line(char** buffer, size_t* length, size_t* capacity, const char* text);
static char* build_list_payload(const char* current_dir, const char* root_dir);
static int handle_echo(int fd, const char* current_dir, const char* root_dir, const char* command);
static int handle_info(int fd, const char* current_dir, const char* root_dir);
static int handle_quit(int fd, const char* current_dir, const char* root_dir);
static int handle_cd(int fd, char* current_dir, const char* root_dir, const char* command);
static int handle_list(int fd, const char* current_dir, const char* root_dir);
static int process_command(int fd, char* current_dir, const char* root_dir, const char* command);
static void* client_thread(void* arg);

int main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <root_dir> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* root_dir = realpath(argv[1], NULL);
    if (root_dir == NULL) {
        perror("Failed to resolve server root");
        return EXIT_FAILURE;
    }

    struct stat root_stat;
    if (stat(root_dir, &root_stat) != 0 || !S_ISDIR(root_stat.st_mode)) {
        fprintf(stderr, "Server root must be an existing directory.\n");
        free(root_dir);
        return EXIT_FAILURE;
    }

    char* end_ptr = NULL;
    long port_value = strtol(argv[2], &end_ptr, 10);
    if (end_ptr == argv[2] || *end_ptr != '\0' || port_value <= 0 || port_value > 65535) {
        fprintf(stderr, "Port must be an integer in range 1..65535.\n");
        free(root_dir);
        return EXIT_FAILURE;
    }

    server_info = read_server_info();
    if (server_info == NULL) {
        free(root_dir);
        return EXIT_FAILURE;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("Failed to create server socket");
        free(server_info);
        free(root_dir);
        return EXIT_FAILURE;
    }

    int opt_value = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(opt_value)) != 0) {
        perror("Failed to set SO_REUSEADDR");
        close(listen_fd);
        free(server_info);
        free(root_dir);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t)port_value);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Failed to bind server socket");
        close(listen_fd);
        free(server_info);
        free(root_dir);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, 32) != 0) {
        perror("Failed to listen on server socket");
        close(listen_fd);
        free(server_info);
        free(root_dir);
        return EXIT_FAILURE;
    }

    log_server_event("server started");
    log_server_event("root=%s", root_dir);
    log_server_event("port=%ld", port_value);

    while (!stop_server) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int clientfd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);

        if (clientfd == -1) {
            if (errno == EINTR && stop_server) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (stop_server && (errno == EBADF || errno == EINVAL)) {
                break;
            }
            perror("Failed to accept client connection");
            continue;
        }

        ClientArgs* args = malloc(sizeof(*args));
        if (args == NULL) {
            perror("Failed to allocate client context");
            close(clientfd);
            continue;
        }

        args->clientfd = clientfd;
        args->client_port = ntohs(client_addr.sin_port);
        args->root_dir = root_dir;
        if (inet_ntop(AF_INET, &client_addr.sin_addr, args->client_ip, sizeof(args->client_ip)) == NULL) {
            snprintf(args->client_ip, sizeof(args->client_ip), "unknown");
        }

        char client_ip[INET_ADDRSTRLEN];
        snprintf(client_ip, sizeof(client_ip), "%s", args->client_ip);
        int client_port = args->client_port;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_thread, args) != 0) {
            perror("Failed to create client thread");
            close(clientfd);
            free(args);
            continue;
        }

        pthread_detach(thread_id);
        log_server_event("client connected %s:%d", client_ip, client_port);
    }

    if (listen_fd != -1) {
        close(listen_fd);
        listen_fd = -1;
    }

    close_all_clients();
    log_server_event("server stopped");

    free(server_info);
    free(root_dir);
    return 0;
}

static void handle_signal(int sig)
{
    (void)sig;
    stop_server = 1;
    if (listen_fd != -1) {
        close(listen_fd);
        listen_fd = -1;
    }
}

static void format_timestamp(char* buffer, size_t buffer_size)
{
    struct timespec ts;
    struct tm local_tm;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &local_tm);
    strftime(buffer, buffer_size, "%Y.%m.%d-%H:%M:%S", &local_tm);

    size_t used = strlen(buffer);
    if (used < buffer_size) {
        snprintf(buffer + used, buffer_size - used, ".%03ld", ts.tv_nsec / 1000000);
    }
}

static void log_server_event(const char* fmt, ...)
{
    char timestamp[64];
    char message[COMMAND_SIZE];
    va_list args;

    format_timestamp(timestamp, sizeof(timestamp));
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    printf("%s %s\n", timestamp, message);
    fflush(stdout);
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
            perror("Failed to write to client socket");
            return -1;
        }
        sent += (size_t)written;
    }
    return 0;
}

static int send_response(int fd, const char* status, const char* current_dir,
    const char* root_dir, const char* payload)
{
    char pwd[PATH_MAX];
    char header[PATH_MAX + 64];

    get_prompt_path(current_dir, root_dir, pwd, sizeof(pwd));
    snprintf(header, sizeof(header), "STATUS %s\nPWD %s\n\n", status, pwd);

    if (send_all(fd, header, strlen(header)) != 0) {
        return -1;
    }
    if (payload != NULL && payload[0] != '\0' && send_all(fd, payload, strlen(payload)) != 0) {
        return -1;
    }
    if (send_all(fd, RESPONSE_END, strlen(RESPONSE_END)) != 0) {
        return -1;
    }

    return 0;
}

static void add_client(int clientfd, pthread_t thread)
{
    ClientNode* node = malloc(sizeof(*node));
    if (node == NULL) {
        perror("Failed to track client connection");
        close(clientfd);
        return;
    }

    node->clientfd = clientfd;
    node->thread = thread;

    pthread_mutex_lock(&clients_mutex);
    node->next = clients;
    clients = node;
    pthread_mutex_unlock(&clients_mutex);
}

static void remove_client(pthread_t thread)
{
    pthread_mutex_lock(&clients_mutex);

    ClientNode** current = &clients;
    while (*current != NULL) {
        if (pthread_equal((*current)->thread, thread)) {
            ClientNode* node = *current;
            *current = node->next;
            free(node);
            break;
        }
        current = &(*current)->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

static void close_all_clients(void)
{
    pthread_mutex_lock(&clients_mutex);

    ClientNode* current = clients;
    while (current != NULL) {
        shutdown(current->clientfd, SHUT_RDWR);
        close(current->clientfd);
        current = current->next;
    }

    while (clients != NULL) {
        ClientNode* node = clients;
        clients = clients->next;
        free(node);
    }

    pthread_mutex_unlock(&clients_mutex);
}

static char* duplicate_string(const char* text)
{
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static char* read_file_contents(const char* path)
{
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char* content = malloc((size_t)file_size + 1);
    if (content == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(content, 1, (size_t)file_size, file);
    fclose(file);
    if (read_bytes != (size_t)file_size) {
        free(content);
        return NULL;
    }

    content[read_bytes] = '\0';
    while (read_bytes > 0 && (content[read_bytes - 1] == '\n' || content[read_bytes - 1] == '\r')) {
        content[read_bytes - 1] = '\0';
        --read_bytes;
    }

    return content;
}

static char* read_server_info(void)
{
    static const char* candidates[] = {
        "server_info.txt",
        "../server_info.txt",
        "../../server_info.txt",
        NULL
    };

    for (size_t i = 0; candidates[i] != NULL; ++i) {
        char* content = read_file_contents(candidates[i]);
        if (content != NULL) {
            return content;
        }
    }

    perror("Failed to open server_info.txt");
    return duplicate_string("Welcome to myserver");
}

static int path_within_root(const char* path, const char* root_dir)
{
    size_t root_len = strlen(root_dir);
    if (strncmp(path, root_dir, root_len) != 0) {
        return 0;
    }

    return path[root_len] == '\0' || path[root_len] == '/';
}

static void get_root_relative(const char* path, const char* root_dir,
    char* buffer, size_t buffer_size)
{
    if (strcmp(path, root_dir) == 0) {
        snprintf(buffer, buffer_size, "/");
        return;
    }

    const char* suffix = path + strlen(root_dir);
    while (*suffix == '/') {
        ++suffix;
    }
    if (*suffix == '\0') {
        snprintf(buffer, buffer_size, "/");
    } else {
        snprintf(buffer, buffer_size, "%s", suffix);
    }
}

static void get_prompt_path(const char* current_dir, const char* root_dir,
    char* buffer, size_t buffer_size)
{
    get_root_relative(current_dir, root_dir, buffer, buffer_size);
}

static int split_path_components(const char* path,
    char components[MAX_COMPONENTS][NAME_MAX + 1])
{
    char copy[PATH_MAX];
    if (snprintf(copy, sizeof(copy), "%s", path) >= (int)sizeof(copy)) {
        return -1;
    }

    int count = 0;
    char* save_ptr = NULL;
    char* token = strtok_r(copy, "/", &save_ptr);
    while (token != NULL) {
        if (count >= MAX_COMPONENTS) {
            return -1;
        }
        snprintf(components[count], NAME_MAX + 1, "%s", token);
        ++count;
        token = strtok_r(NULL, "/", &save_ptr);
    }

    return count;
}

static int build_relative_path(const char* from_dir, const char* to_path,
    char* buffer, size_t buffer_size)
{
    char from_components[MAX_COMPONENTS][NAME_MAX + 1];
    char to_components[MAX_COMPONENTS][NAME_MAX + 1];
    int from_count = split_path_components(from_dir, from_components);
    int to_count = split_path_components(to_path, to_components);
    if (from_count < 0 || to_count < 0) {
        return -1;
    }

    int common = 0;
    while (common < from_count && common < to_count &&
        strcmp(from_components[common], to_components[common]) == 0) {
        ++common;
    }

    char result[PATH_MAX];
    result[0] = '\0';

    for (int i = common; i < from_count; ++i) {
        if (append_part(result, sizeof(result), "../") != 0) {
            return -1;
        }
    }

    for (int i = common; i < to_count; ++i) {
        if (append_part(result, sizeof(result), to_components[i]) != 0) {
            return -1;
        }
        if (i + 1 < to_count && append_part(result, sizeof(result), "/") != 0) {
            return -1;
        }
    }

    if (result[0] == '\0') {
        snprintf(result, sizeof(result), ".");
    }

    snprintf(buffer, buffer_size, "%s", result);
    return 0;
}

static int normalize_target_path(const char* root_dir, const char* base_dir,
    const char* raw_target, char* buffer, size_t buffer_size)
{
    char combined[PATH_MAX];
    if (raw_target[0] == '/') {
        if (snprintf(combined, sizeof(combined), "%s%s", root_dir, raw_target) >= (int)sizeof(combined)) {
            return -1;
        }
    } else {
        if (snprintf(combined, sizeof(combined), "%s/%s", base_dir, raw_target) >= (int)sizeof(combined)) {
            return -1;
        }
    }

    char components[MAX_COMPONENTS][NAME_MAX + 1];
    int count = 0;

    char copy[PATH_MAX];
    snprintf(copy, sizeof(copy), "%s", combined);
    char* save_ptr = NULL;
    char* token = strtok_r(copy, "/", &save_ptr);
    while (token != NULL) {
        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            token = strtok_r(NULL, "/", &save_ptr);
            continue;
        }
        if (strcmp(token, "..") == 0) {
            if (count > 0) {
                --count;
            }
            token = strtok_r(NULL, "/", &save_ptr);
            continue;
        }
        if (count >= MAX_COMPONENTS) {
            return -1;
        }
        snprintf(components[count], NAME_MAX + 1, "%s", token);
        ++count;
        token = strtok_r(NULL, "/", &save_ptr);
    }

    char normalized[PATH_MAX];
    snprintf(normalized, sizeof(normalized), "/");
    for (int i = 0; i < count; ++i) {
        if (append_part(normalized, sizeof(normalized), components[i]) != 0) {
            return -1;
        }
        if (i + 1 < count && append_part(normalized, sizeof(normalized), "/") != 0) {
            return -1;
        }
    }

    if (!path_within_root(normalized, root_dir)) {
        return -1;
    }

    snprintf(buffer, buffer_size, "%s", normalized);
    return 0;
}

static void get_target_display_path(const char* current_dir, const char* root_dir,
    const char* target_path, char* buffer, size_t buffer_size)
{
    char relative[PATH_MAX];
    if (build_relative_path(current_dir, target_path, relative, sizeof(relative)) == 0 &&
        strcmp(relative, "..") != 0 && strncmp(relative, "../", 3) != 0) {
        if (strcmp(relative, ".") == 0) {
            snprintf(buffer, buffer_size, "/");
        } else {
            snprintf(buffer, buffer_size, "%s", relative);
        }
        return;
    }

    char root_relative[PATH_MAX];
    get_root_relative(target_path, root_dir, root_relative, sizeof(root_relative));
    if (strcmp(root_relative, "/") == 0) {
        snprintf(buffer, buffer_size, "/");
    } else {
        snprintf(buffer, buffer_size, "/%s", root_relative);
    }
}

static int append_part(char* buffer, size_t buffer_size, const char* part)
{
    size_t current_length = strlen(buffer);
    size_t part_length = strlen(part);
    if (current_length + part_length + 1 > buffer_size) {
        return -1;
    }

    memcpy(buffer + current_length, part, part_length + 1);
    return 0;
}

static int append_text(char** buffer, size_t* length, size_t* capacity, const char* text)
{
    size_t text_length = strlen(text);
    if (*length + text_length + 1 > *capacity) {
        size_t new_capacity = *capacity;
        while (*length + text_length + 1 > new_capacity) {
            new_capacity *= 2;
        }

        char* resized = realloc(*buffer, new_capacity);
        if (resized == NULL) {
            return -1;
        }
        *buffer = resized;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *length, text, text_length + 1);
    *length += text_length;
    return 0;
}

static int append_line(char** buffer, size_t* length, size_t* capacity, const char* text)
{
    if (append_text(buffer, length, capacity, text) != 0) {
        return -1;
    }
    return append_text(buffer, length, capacity, "\n");
}

static char* build_list_payload(const char* current_dir, const char* root_dir)
{
    DIR* dir = opendir(current_dir);
    if (dir == NULL) {
        return NULL;
    }

    size_t capacity = 256;
    size_t length = 0;
    char* payload = malloc(capacity);
    if (payload == NULL) {
        closedir(dir);
        return NULL;
    }
    payload[0] = '\0';

    struct dirent* entry;
    int has_entries = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, entry->d_name) >= (int)sizeof(full_path)) {
            continue;
        }

        struct stat st;
        if (lstat(full_path, &st) != 0) {
            continue;
        }

        char line[PATH_MAX * 2];
        if (S_ISDIR(st.st_mode)) {
            snprintf(line, sizeof(line), "%s/", entry->d_name);
        } else if (S_ISLNK(st.st_mode)) {
            char raw_target[PATH_MAX];
            ssize_t target_len = readlink(full_path, raw_target, sizeof(raw_target) - 1);
            if (target_len < 0) {
                snprintf(line, sizeof(line), "%s", entry->d_name);
            } else {
                raw_target[target_len] = '\0';
                char normalized_target[PATH_MAX];
                if (normalize_target_path(root_dir, current_dir, raw_target,
                        normalized_target, sizeof(normalized_target)) != 0) {
                    snprintf(line, sizeof(line), "%s", entry->d_name);
                } else {
                    struct stat target_stat;
                    if (lstat(normalized_target, &target_stat) != 0) {
                        snprintf(line, sizeof(line), "%s", entry->d_name);
                    } else {
                        char target_display[PATH_MAX];
                        get_target_display_path(current_dir, root_dir, normalized_target,
                            target_display, sizeof(target_display));

                        if (S_ISLNK(target_stat.st_mode)) {
                            snprintf(line, sizeof(line), "%s -->> %s", entry->d_name, target_display);
                        } else if (S_ISDIR(target_stat.st_mode)) {
                            snprintf(line, sizeof(line), "%s --> %s/", entry->d_name, target_display);
                        } else {
                            snprintf(line, sizeof(line), "%s --> %s", entry->d_name, target_display);
                        }
                    }
                }
            }
        } else {
            snprintf(line, sizeof(line), "%s", entry->d_name);
        }

        if (append_line(&payload, &length, &capacity, line) != 0) {
            free(payload);
            closedir(dir);
            return NULL;
        }
        has_entries = 1;
    }

    closedir(dir);

    if (!has_entries) {
        if (append_text(&payload, &length, &capacity, "(empty)") != 0) {
            free(payload);
            return NULL;
        }
    } else if (length > 0 && payload[length - 1] == '\n') {
        payload[length - 1] = '\0';
    }

    return payload;
}

static int handle_echo(int fd, const char* current_dir, const char* root_dir, const char* command)
{
    const char* payload = command + 4;
    while (*payload == ' ') {
        ++payload;
    }

    char echo_buffer[COMMAND_SIZE];
    if (payload[0] == '"' && payload[1] != '\0') {
        size_t length = strlen(payload);
        if (length >= 2 && payload[length - 1] == '"') {
            size_t copy_length = length - 2;
            if (copy_length >= sizeof(echo_buffer)) {
                copy_length = sizeof(echo_buffer) - 1;
            }
            memcpy(echo_buffer, payload + 1, copy_length);
            echo_buffer[copy_length] = '\0';
            return send_response(fd, "OK", current_dir, root_dir, echo_buffer);
        }
    }

    return send_response(fd, "OK", current_dir, root_dir, payload);
}

static int handle_info(int fd, const char* current_dir, const char* root_dir)
{
    return send_response(fd, "OK", current_dir, root_dir, server_info);
}

static int handle_quit(int fd, const char* current_dir, const char* root_dir)
{
    return send_response(fd, "BYE", current_dir, root_dir, "BYE");
}

static int handle_cd(int fd, char* current_dir, const char* root_dir, const char* command)
{
    const char* requested = command + 2;
    while (*requested == ' ') {
        ++requested;
    }

    if (*requested == '\0') {
        return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: directory is not specified");
    }

    char candidate[PATH_MAX];
    if (requested[0] == '/') {
        if (snprintf(candidate, sizeof(candidate), "%s%s", root_dir, requested) >= (int)sizeof(candidate)) {
            return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: path is too long");
        }
    } else {
        if (snprintf(candidate, sizeof(candidate), "%s/%s", current_dir, requested) >= (int)sizeof(candidate)) {
            return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: path is too long");
        }
    }

    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) == NULL) {
        if (errno == ENOENT) {
            return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: directory does not exist");
        }
        return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: cannot resolve directory");
    }

    if (!path_within_root(resolved, root_dir)) {
        return send_response(fd, "OK", current_dir, root_dir, "");
    }

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: target is not a directory");
    }

    snprintf(current_dir, PATH_MAX, "%s", resolved);
    return send_response(fd, "OK", current_dir, root_dir, "");
}

static int handle_list(int fd, const char* current_dir, const char* root_dir)
{
    char* payload = build_list_payload(current_dir, root_dir);
    if (payload == NULL) {
        return send_response(fd, "ERROR", current_dir, root_dir, "ERROR: failed to build directory listing");
    }

    int result = send_response(fd, "OK", current_dir, root_dir, payload);
    free(payload);
    return result;
}

static int process_command(int fd, char* current_dir, const char* root_dir, const char* command)
{
    if (strcmp(command, "INFO") == 0) {
        return handle_info(fd, current_dir, root_dir);
    }
    if (strcmp(command, "LIST") == 0) {
        return handle_list(fd, current_dir, root_dir);
    }
    if (strcmp(command, "QUIT") == 0) {
        return handle_quit(fd, current_dir, root_dir);
    }
    if (strncmp(command, "ECHO", 4) == 0 &&
        (command[4] == '\0' || command[4] == ' ')) {
        return handle_echo(fd, current_dir, root_dir, command);
    }
    if (strncmp(command, "CD", 2) == 0 &&
        (command[2] == '\0' || command[2] == ' ')) {
        return handle_cd(fd, current_dir, root_dir, command);
    }

    return send_response(fd, "ERROR", current_dir, root_dir,
        "ERROR: unknown command. Available commands: ECHO, INFO, CD, LIST, QUIT");
}

static void* client_thread(void* arg)
{
    ClientArgs* client_args = arg;
    int client_socket = client_args->clientfd;
    const char* root_dir = client_args->root_dir;
    char client_ip[INET_ADDRSTRLEN];
    int client_port = client_args->client_port;
    char current_dir[PATH_MAX];
    char command[COMMAND_SIZE];

    snprintf(client_ip, sizeof(client_ip), "%s", client_args->client_ip);
    snprintf(current_dir, sizeof(current_dir), "%s", root_dir);
    free(client_args);

    pthread_t self = pthread_self();
    add_client(client_socket, self);

    if (send_response(client_socket, "OK", current_dir, root_dir, server_info) != 0) {
        goto cleanup;
    }

    int read_fd = dup(client_socket);
    if (read_fd == -1) {
        perror("Failed to duplicate client socket");
        goto cleanup;
    }

    FILE* stream = fdopen(read_fd, "r");
    if (stream == NULL) {
        perror("Failed to open client stream");
        close(read_fd);
        goto cleanup;
    }

    while (!stop_server && fgets(command, sizeof(command), stream) != NULL) {
        command[strcspn(command, "\r\n")] = '\0';
        if (command[0] == '\0') {
            continue;
        }

        log_server_event("%s:%d %s", client_ip, client_port, command);
        if (process_command(client_socket, current_dir, root_dir, command) != 0) {
            break;
        }
        if (strcmp(command, "QUIT") == 0) {
            break;
        }
    }

    fclose(stream);

cleanup:
    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    remove_client(self);
    log_server_event("client disconnected %s:%d", client_ip, client_port);
    return NULL;
}
