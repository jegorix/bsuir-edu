#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>

#define MAX_CHILDS 20

typedef struct {
	pid_t pid;
	bool printEnabled;
} ChildInfo;

void spawnChildProcess(ChildInfo* childs, int* childCount, const char* childPath);
void terminateLastChild(ChildInfo* childs, int* childCount);
void printProcessList(const ChildInfo* childs, int childCount);
void terminateAllChildren(ChildInfo* childs, int* childCount);
void toggleChildOutput(ChildInfo* childs, int childCount);
int resolveChildPath(const char* argv0, char* childPath, size_t childPathSize);


int main(int argc, char** argv) {
	(void)argc;
	ChildInfo childs[MAX_CHILDS];
	int childCount = 0;
	char input[32];
	char option;
	char childPath[PATH_MAX];

	if (resolveChildPath(argv[0], childPath, sizeof(childPath)) != 0) {
		fprintf(stderr, "Failed to resolve child path\n");
		return EXIT_FAILURE;
	}

	while (1) {
		option = '\0';

		printf("\nChoose variant:\n");
		printf("+ - create child process\n");
		printf("- - kill last child process;\n");
		printf("t - toggle child output;\n");
		printf("l - list all processes;\n");
		printf("k - kill all child processes;\n");
		printf("q - quit with killing all child processes;\n");

		if (!fgets(input, sizeof(input), stdin)) {
			option = 'q';
		} else {
			option = input[0];
		}

		switch (option) {
			case '+': spawnChildProcess(childs, &childCount, childPath); break;
			case '-': terminateLastChild(childs, &childCount); break;
			case 't': toggleChildOutput(childs, childCount); break;
			case 'l': printProcessList(childs, childCount); break;
			case 'k':
			case 'q': terminateAllChildren(childs, &childCount); break;
			case '\n':
			case '\0': break;
			default: printf("Wrong input. Use +, -, t, l, k or q\n");
		}

		if (option == 'q')
			break;
	}
	printf("Program finished\n");
	return 0;
}


int resolveChildPath(const char* argv0, char* childPath, size_t childPathSize) {
	char parentPath[PATH_MAX];
	char* slashPos;

	if (snprintf(parentPath, sizeof(parentPath), "%s", argv0) >= (int)sizeof(parentPath))
		return -1;

	slashPos = strrchr(parentPath, '/');
	if (!slashPos)
		return snprintf(childPath, childPathSize, "./child") >= (int)childPathSize ? -1 : 0;

	*slashPos = '\0';
	return snprintf(childPath, childPathSize, "%s/child", parentPath) >= (int)childPathSize ? -1 : 0;
}


void spawnChildProcess(ChildInfo* childs, int* childCount, const char* childPath) {
	if (*childCount >= MAX_CHILDS) {
		printf("Maximum child processes reached (%d)\n", MAX_CHILDS);
		return;
	}

	pid_t new_pid = fork();
	if (new_pid < 0) {
		perror("Error. Fork failed");
		return;
	}

	if (new_pid == 0) {
		execl(childPath, childPath, NULL);
		perror("execl failed");
		exit(EXIT_FAILURE);
	} else {
		childs[*childCount].pid = new_pid;
		childs[*childCount].printEnabled = true;
		printf("Parent: PID=%d | C_%02d (PID=%d) created\n", getpid(), *childCount, new_pid);
		(*childCount)++;
	}
}

void terminateLastChild(ChildInfo* childs, int* childCount) {
	if (*childCount == 0) {
		printf("No children to terminate\n");
		return;
	}

	pid_t last_pid = childs[*childCount - 1].pid;
	kill(last_pid, SIGUSR1);
	waitpid(last_pid, NULL, 0);
	(*childCount)--;
	printf("Parent: PID=%d | C_%02d (PID=%d) terminated | Childs remaining: %d\n",
		getpid(),
		*childCount,
		last_pid,
		*childCount);
}

void printProcessList(const ChildInfo* childs, int childCount) {
	printf("\nParent: PID = %d\n", getpid());
	if (childCount == 0) {
		printf("No child processes\n");
		return;
	}

	for (int i = 0; i < childCount; i++) {
		printf("C_%02d: PID = %d | %s\n", i, childs[i].pid, childs[i].printEnabled ? "ON" : "OFF");
	}
}

void terminateAllChildren(ChildInfo* childs, int* childCount) {
	if (*childCount == 0) {
		printf("No children to terminate\n");
		return;
	}

	while (*childCount > 0) {
		pid_t pid = childs[*childCount - 1].pid;
		kill(pid, SIGUSR1);
		waitpid(pid, NULL, 0);
		(*childCount)--;
	}
	printf("Parent: PID=%d | All children terminated\n", getpid());
}

void toggleChildOutput(ChildInfo* childs, int childCount) {
	char input[32];
	char* endPtr;
	long childIndex;

	if (childCount == 0) {
		printf("No children to toggle\n");
		return;
	}

	printf("Enter child number: ");
	if (!fgets(input, sizeof(input), stdin)) {
		printf("Failed to read child number\n");
		return;
	}

	childIndex = strtol(input, &endPtr, 10);
	if (endPtr == input || (*endPtr != '\n' && *endPtr != '\0')) {
		printf("Wrong child number\n");
		return;
	}

	if (childIndex < 0 || childIndex >= childCount) {
		printf("Child index out of range\n");
		return;
	}

	if (kill(childs[childIndex].pid, SIGUSR2) != 0) {
		perror("Failed to toggle child output");
		return;
	}

	childs[childIndex].printEnabled = !childs[childIndex].printEnabled;
	printf("C_%02ld (PID = %d) output %s\n",
		childIndex,
		childs[childIndex].pid,
		childs[childIndex].printEnabled ? "ON" : "OFF");
}
