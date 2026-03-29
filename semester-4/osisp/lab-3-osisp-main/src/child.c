#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#define REPORT_ITERATIONS 101
#define TIMER_INTERVAL_USEC 10000

static volatile sig_atomic_t terminate_requested = 0;
static volatile sig_atomic_t print_enabled = 1;
static volatile sig_atomic_t timer_fired = 0;

struct Pair
{
    int first;
    int second;
};

void handleSignal(int sig);
int installSignalHandlers(void);
int armTimer(void);
void disarmTimer(void);
void updateStats(struct Pair pair, int* stats);


int main(void)
{
    if (installSignalHandlers() != 0)
    {
        perror("sigaction failed");
        return EXIT_FAILURE;
    }

    while (!terminate_requested)
    {
        int stats[4] = {0, 0, 0, 0};
        int iteration = 0;

        while (iteration < REPORT_ITERATIONS && !terminate_requested)
        {
            struct Pair pair = {0, 0};
            timer_fired = 0;

            if (armTimer() != 0)
            {
                perror("setitimer failed");
                return EXIT_FAILURE;
            }

            while (!timer_fired && !terminate_requested)
            {
                pair.first = 0;

                if (timer_fired || terminate_requested)
                    break;

                pair.second = 0;

                if (timer_fired || terminate_requested)
                    break;

                pair.first = 1;
                if (timer_fired || terminate_requested)
                    break;

                pair.second = 1;
            }

            disarmTimer();

            if (terminate_requested)
                break;

            updateStats(pair, stats);
            iteration++;
        }

        if (terminate_requested)
            break;

        if (print_enabled)
        {
            printf("Child: PPID=%d PID=%d", getppid(), getpid());
            printf(" | (0,0): %d | (0,1): %d | (1,0): %d | (1,1): %d\n", stats[0], stats[1], stats[2], stats[3]);
            fflush(stdout);
        }
    }
    return 0;
}


void handleSignal(int sig)
{
    if (sig == SIGUSR1)
        terminate_requested = 1;
    else if (sig == SIGUSR2)
        print_enabled = !print_enabled;
    else if (sig == SIGALRM)
        timer_fired = 1;
}

int installSignalHandlers(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = handleSignal;
    if (sigaction(SIGUSR1, &sa, NULL) != 0)
        return -1;
    if (sigaction(SIGUSR2, &sa, NULL) != 0)
        return -1;
    if (sigaction(SIGALRM, &sa, NULL) != 0)
        return -1;

    return 0;
}

int armTimer(void)
{
    struct itimerval timer =
    {
        .it_interval = {0, 0},
        .it_value = {0, TIMER_INTERVAL_USEC}
    };

    return setitimer(ITIMER_REAL, &timer, NULL);
}

void disarmTimer(void)
{
    struct itimerval timer = {0};

    setitimer(ITIMER_REAL, &timer, NULL);
}

void updateStats(struct Pair pair, int* stats)
{
    int index = pair.first * 2 + pair.second;

    if (index >= 0 && index < 4)
    {
        stats[index]++;
    }
}
