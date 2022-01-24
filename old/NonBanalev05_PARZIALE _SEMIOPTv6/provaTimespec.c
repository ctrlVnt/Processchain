#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <sys/sysinfo.h>

int main(void) {
    /* get uptime in seconds */
    struct sysinfo info;
    sysinfo(&info);

    /* calculate boot time in seconds since the Epoch */
    const time_t boottime = time(NULL) - info.uptime;

    /* get monotonic clock time */
    struct timespec monotime;
    clock_gettime(CLOCK_MONOTONIC, &monotime);

    /* calculate current time in seconds since the Epoch */
    time_t curtime = boottime + monotime.tv_sec;

    /* get realtime clock time for comparison */
    struct timespec realtime;
    clock_gettime(CLOCK_REALTIME, &realtime);

    printf("Boot time = %s", ctime(&boottime));
    printf("Current time = %s", ctime(&curtime));
    printf("Real Time = %s", ctime(&realtime.tv_sec));

    return 0;
}

void creaPoiInviaTransazione()
{
    
}