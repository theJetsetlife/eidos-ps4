/* A separately loaded host library must bind the host socket, not DS code. */
#include <sys/socket.h>
#include <unistd.h>

int eidos_test_host_library_socket(void)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    close(s);
    return 0;
}
