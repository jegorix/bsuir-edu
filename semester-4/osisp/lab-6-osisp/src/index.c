#define _POSIX_C_SOURCE 200809L

#include "index.h"

#include <stdlib.h>
#include <time.h>

enum
{
    FIRST_VALID_MJD = 15020,
    UNIX_EPOCH_MJD = 40587
};

static uint64_t get_yesterday_mjd(void)
{
    time_t now = time(NULL);
    uint64_t today = (uint64_t)(now / 86400) + UNIX_EPOCH_MJD;

    if (today == 0)
    {
        return FIRST_VALID_MJD;
    }

    return today - 1U;
}

double generate_random_time_mark(unsigned int* seed)
{
    uint64_t yesterday = get_yesterday_mjd();
    uint64_t span = yesterday - FIRST_VALID_MJD + 1U;
    uint64_t whole = FIRST_VALID_MJD + (uint64_t)(rand_r(seed) % span);
    double fraction = (double)rand_r(seed) / ((double)RAND_MAX + 1.0);

    return (double)whole + fraction;
}

int compare_index_records(const void* left, const void* right)
{
    const IndexRecord* lhs = (const IndexRecord*)left;
    const IndexRecord* rhs = (const IndexRecord*)right;

    if (lhs->time_mark < rhs->time_mark)
    {
        return -1;
    }
    if (lhs->time_mark > rhs->time_mark)
    {
        return 1;
    }
    if (lhs->recno < rhs->recno)
    {
        return -1;
    }
    if (lhs->recno > rhs->recno)
    {
        return 1;
    }
    return 0;
}
