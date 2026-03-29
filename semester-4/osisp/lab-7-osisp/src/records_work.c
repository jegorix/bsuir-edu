#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "record.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define COMMAND_BUFFER_SIZE 128

typedef struct
{
    int has_record;
    int current_record_no;
    Record original_record;
    Record working_record;
} SessionState;

static void print_error_and_exit(const char* message);
static off_t get_record_offset(int record_no);
static struct flock make_record_lock(int record_no, short lock_type);
static void lock_record(int fd, int record_no, short lock_type, int blocking);
static void unlock_record(int fd, int record_no);
static int query_lock(int fd, int record_no, short desired_lock, struct flock* result);

static void read_record_unlocked(int fd, int record_no, Record* record);
static void write_record_unlocked(int fd, int record_no, const Record* record);
static void load_record_with_read_lock(int fd, int record_no, Record* record);
static void list_records(int fd, int total_records);
static void modify_record(Record* record);
static int save_record(int fd, SessionState* state);
static void show_record_info(int fd, const SessionState* state);

static off_t get_file_size(int fd);
static int read_line(char* buffer, size_t size);
static void trim_newline(char* str);
static void discard_rest_of_line(void);
static int parse_record_number(const char* command, int* record_no);
static int record_equals(const Record* left, const Record* right);

static void print_menu(int total_records, const SessionState* state);
static void print_record_line(int record_no, const Record* record);
static void print_record_details(const char* title, const Record* record);

int main(void)
{
    int fd = open(FILENAME, O_RDWR);
    SessionState state;
    off_t file_size;
    int total_records;
    char command[COMMAND_BUFFER_SIZE];

    if (fd == -1)
    {
        if (errno == ENOENT)
        {
            fprintf(stderr, "File \"%s\" was not found.\n", FILENAME);
            fprintf(stderr, "Create it with the generator first.\n");
            return EXIT_FAILURE;
        }
        print_error_and_exit("open");
    }

    file_size = get_file_size(fd);
    if (file_size <= 0 || (file_size % RECORD_SIZE) != 0)
    {
        fprintf(stderr, "File \"%s\" has an invalid size for fixed-size records.\n", FILENAME);
        close(fd);
        return EXIT_FAILURE;
    }

    total_records = (int)(file_size / RECORD_SIZE);
    if (total_records < 10)
    {
        fprintf(stderr, "File \"%s\" must contain at least 10 records.\n", FILENAME);
        close(fd);
        return EXIT_FAILURE;
    }

    memset(&state, 0, sizeof(state));

    while (1)
    {
        int record_no = 0;

        print_menu(total_records, &state);
        if (!read_line(command, sizeof(command)))
        {
            printf("\nExiting.\n");
            break;
        }

        if (strcmp(command, "lst") == 0)
        {
            list_records(fd, total_records);
            continue;
        }

        if (parse_record_number(command, &record_no))
        {
            if (record_no < 1 || record_no > total_records)
            {
                printf("Record number must be in range [1, %d].\n", total_records);
                continue;
            }

            load_record_with_read_lock(fd, record_no, &state.working_record);
            state.original_record = state.working_record;
            state.current_record_no = record_no;
            state.has_record = 1;

            printf("\nRecord %d was loaded.\n", record_no);
            print_record_details("Current working record:", &state.working_record);
            continue;
        }

        if (strcmp(command, "mod") == 0)
        {
            if (!state.has_record)
            {
                printf("Load a record first with `get <number>`.\n");
                continue;
            }

            modify_record(&state.working_record);
            printf("Record %d was modified in memory.\n", state.current_record_no);
            print_record_details("Updated working record:", &state.working_record);
            continue;
        }

        if (strcmp(command, "put") == 0)
        {
            if (!state.has_record)
            {
                printf("Load a record first with `get <number>`.\n");
                continue;
            }

            if (save_record(fd, &state) == 0)
            {
                printf("Record %d was saved.\n", state.current_record_no);
            }
            continue;
        }

        if (strcmp(command, "inf") == 0)
        {
            if (!state.has_record)
            {
                printf("Load a record first with `get <number>`.\n");
                continue;
            }

            show_record_info(fd, &state);
            continue;
        }

        if (strcmp(command, "q") == 0 || strcmp(command, "quit") == 0)
        {
            break;
        }

        printf("Unknown command. Use: lst, get N, mod, put, inf, q.\n");
    }

    close(fd);
    return EXIT_SUCCESS;
}

static void print_error_and_exit(const char* message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static off_t get_record_offset(int record_no)
{
    return (off_t)(record_no - 1) * RECORD_SIZE;
}

static struct flock make_record_lock(int record_no, short lock_type)
{
    struct flock lock;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = get_record_offset(record_no);
    lock.l_len = RECORD_SIZE;
    lock.l_pid = 0;
    return lock;
}

static void lock_record(int fd, int record_no, short lock_type, int blocking)
{
    struct flock lock = make_record_lock(record_no, lock_type);
    int command = blocking ? F_OFD_SETLKW : F_OFD_SETLK;

    if (fcntl(fd, command, &lock) == -1)
    {
        print_error_and_exit("fcntl lock");
    }
}

static void unlock_record(int fd, int record_no)
{
    struct flock lock = make_record_lock(record_no, F_UNLCK);

    if (fcntl(fd, F_OFD_SETLK, &lock) == -1)
    {
        print_error_and_exit("fcntl unlock");
    }
}

static int query_lock(int fd, int record_no, short desired_lock, struct flock* result)
{
    *result = make_record_lock(record_no, desired_lock);
    return fcntl(fd, F_OFD_GETLK, result);
}

static void read_record_unlocked(int fd, int record_no, Record* record)
{
    ssize_t bytes = pread(fd, record, sizeof(*record), get_record_offset(record_no));

    if (bytes != (ssize_t)sizeof(*record))
    {
        print_error_and_exit("pread");
    }
}

static void write_record_unlocked(int fd, int record_no, const Record* record)
{
    ssize_t bytes = pwrite(fd, record, sizeof(*record), get_record_offset(record_no));

    if (bytes != (ssize_t)sizeof(*record))
    {
        print_error_and_exit("pwrite");
    }
}

static void load_record_with_read_lock(int fd, int record_no, Record* record)
{
    lock_record(fd, record_no, F_RDLCK, 1);
    read_record_unlocked(fd, record_no, record);
    unlock_record(fd, record_no);
}

static void list_records(int fd, int total_records)
{
    printf("\n%-6s %-24s %-34s %-8s\n", "No", "Name", "Address", "Semester");
    printf("-------------------------------------------------------------------------------\n");

    for (int record_no = 1; record_no <= total_records; ++record_no)
    {
        Record record;

        lock_record(fd, record_no, F_RDLCK, 1);
        read_record_unlocked(fd, record_no, &record);
        unlock_record(fd, record_no);
        print_record_line(record_no, &record);
    }

    printf("-------------------------------------------------------------------------------\n");
}

static void modify_record(Record* record)
{
    char buffer[32];
    int semester = 0;

    printf("New name: ");
    if (!read_line(record->name, sizeof(record->name)))
    {
        print_error_and_exit("stdin");
    }

    printf("New address: ");
    if (!read_line(record->address, sizeof(record->address)))
    {
        print_error_and_exit("stdin");
    }

    while (1)
    {
        printf("New semester [1-8]: ");
        if (!read_line(buffer, sizeof(buffer)))
        {
            print_error_and_exit("stdin");
        }

        if (sscanf(buffer, "%d", &semester) == 1 && semester >= 1 && semester <= 8)
        {
            record->semester = (uint8_t)semester;
            return;
        }

        printf("Semester must be a number in range [1, 8].\n");
    }
}

static int save_record(int fd, SessionState* state)
{
    Record latest_record;

    if (record_equals(&state->working_record, &state->original_record))
    {
        printf("There are no local changes to save.\n");
        return -1;
    }

    lock_record(fd, state->current_record_no, F_WRLCK, 1);
    read_record_unlocked(fd, state->current_record_no, &latest_record);

    if (!record_equals(&latest_record, &state->original_record))
    {
        unlock_record(fd, state->current_record_no);
        printf("Record %d was changed by another process after your GET.\n", state->current_record_no);
        state->original_record = latest_record;
        state->working_record = latest_record;
        print_record_details("Current file version was loaded into your workspace:", &latest_record);
        printf("Repeat modification if you still want to change the record.\n");
        return -1;
    }

    write_record_unlocked(fd, state->current_record_no, &state->working_record);
    if (fsync(fd) != 0)
    {
        unlock_record(fd, state->current_record_no);
        print_error_and_exit("fsync");
    }
    unlock_record(fd, state->current_record_no);

    state->original_record = state->working_record;
    return 0;
}

static void show_record_info(int fd, const SessionState* state)
{
    struct flock lock_info;
    Record file_record;

    if (query_lock(fd, state->current_record_no, F_WRLCK, &lock_info) == -1)
    {
        print_error_and_exit("fcntl getlk");
    }

    lock_record(fd, state->current_record_no, F_RDLCK, 1);
    read_record_unlocked(fd, state->current_record_no, &file_record);
    unlock_record(fd, state->current_record_no);

    printf("\nRecord %d information\n", state->current_record_no);
    if (lock_info.l_type == F_UNLCK)
    {
        printf("Lock state: no conflicting writer lock\n");
    }
    else
    {
        printf("Lock state: conflicting %s lock exists\n",
               (lock_info.l_type == F_WRLCK) ? "write" : "read");
    }
    print_record_details("Working version:", &state->working_record);
    print_record_details("Original version from last GET:", &state->original_record);
    print_record_details("Current version in file:", &file_record);
}

static off_t get_file_size(int fd)
{
    struct stat st;

    if (fstat(fd, &st) == -1)
    {
        print_error_and_exit("fstat");
    }

    return st.st_size;
}

static int read_line(char* buffer, size_t size)
{
    if (fgets(buffer, size, stdin) == NULL)
    {
        return 0;
    }

    if (strchr(buffer, '\n') == NULL)
    {
        discard_rest_of_line();
    }
    trim_newline(buffer);
    return 1;
}

static void trim_newline(char* str)
{
    str[strcspn(str, "\n")] = '\0';
}

static void discard_rest_of_line(void)
{
    int ch;

    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }
}

static int parse_record_number(const char* command, int* record_no)
{
    return sscanf(command, "get %d", record_no) == 1;
}

static int record_equals(const Record* left, const Record* right)
{
    return memcmp(left, right, sizeof(*left)) == 0;
}

static void print_menu(int total_records, const SessionState* state)
{
    printf("\n========================================\n");
    printf(" Lab 7: Concurrent Record Access\n");
    printf("========================================\n");
    printf("File: %s\n", FILENAME);
    printf("Total records: %d\n", total_records);
    if (state->has_record)
    {
        printf("Current record: %d\n", state->current_record_no);
    }
    else
    {
        printf("Current record: none\n");
    }
    printf("----------------------------------------\n");
    printf("lst       - list all records\n");
    printf("get N     - load record N (1-based)\n");
    printf("mod       - modify the loaded record\n");
    printf("put       - save the loaded record\n");
    printf("inf       - show working/original/file versions\n");
    printf("q         - quit\n");
    printf("Command: ");
    fflush(stdout);
}

static void print_record_line(int record_no, const Record* record)
{
    printf("%-6d %-24.24s %-34.34s %-8u\n",
           record_no,
           record->name,
           record->address,
           (unsigned int)record->semester);
}

static void print_record_details(const char* title, const Record* record)
{
    printf("%s\n", title);
    printf("  Name     : %s\n", record->name);
    printf("  Address  : %s\n", record->address);
    printf("  Semester : %u\n", (unsigned int)record->semester);
}
