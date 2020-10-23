#include "../kcp/ikcp.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>

constexpr const unsigned long port = 8000;

namespace chrono = std::chrono;

auto getUnixTimestamp()
{
    return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

int createConnection()
{
    puts("Creating socket");

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        err(1, "Failed to create udp socket");

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    printf("Connecting to the target %lu\n", port);
    if (connect(sockfd, (const struct sockaddr*) &addr, sizeof(addr)) < 0)
        err(1, "Failed to connect the socket to %lu", static_cast<unsigned long>(port));

    puts("Sending first packet to the target");

    char buffer[1];
    if (send(sockfd, buffer, sizeof(buffer), 0) < 0)
        err(1, "send failed in %s", __PRETTY_FUNCTION__);

    return sockfd;
}
int output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    int sockfd = *static_cast<int*>(user);
    if (send(sockfd, buf, len, 0) < 0)
        err(1, "send failed in %s", __PRETTY_FUNCTION__);
    return 0;
}
int main(int argc, char* argv[])
{
    int sockfd = createConnection();

    puts("Initializing kcp");

    ikcpcb *kcp = ikcp_create(0, &sockfd);
    ikcp_setoutput(kcp, output);
    ikcp_nodelay(kcp, 1, 20, 2, 1);
    ikcp_wndsize(kcp, 1024, 1024);

    puts("Starts to receive data");

    bool requestTerminate = false;
    bool shouldBreak = false;
    char buffer[500000];
    for (uint64_t byte = 0; !shouldBreak; ) {
        ssize_t cnt = recv(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (cnt < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                err(1, "recv failed in %s", __PRETTY_FUNCTION__);
        } else {
            ikcp_input(kcp, buffer, cnt);
        }
        ikcp_update(kcp, getUnixTimestamp());
        for (int sz; ; ) {
            sz = ikcp_recv(kcp, buffer, sizeof(buffer));
            if (sz > 0) {
                byte += sz;
                if (buffer[0] != 0)
                    shouldBreak = true;
            } else
                break;
        }
        if (!requestTerminate && byte > 1 * 1000 * 1000 * 1000 /* 1 GB */) {
            ikcp_send(kcp, buffer, 1);
            ikcp_update(kcp, getUnixTimestamp());
            ikcp_flush(kcp);

            requestTerminate = true;
        }
    }

    puts("Done");

    ikcp_release(kcp);

    return 0;
}
