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

struct Connection {
    int sockfd;
    struct sockaddr_in addr;
};
struct Connection createConnection()
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

    printf("Binding socket to port %lu\n", port);
    if (bind(sockfd, (const struct sockaddr*) &addr, sizeof(addr)) < 0)
        err(1, "Failed to bind the socket to %lu", static_cast<unsigned long>(port));

    struct Connection conn;
    conn.sockfd = sockfd;
    memset(&conn.addr, 0, sizeof(addr));

    puts("Waiting for first inbound packet");
    char buffer[1];
    socklen_t len = sizeof(conn.addr);
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &conn.addr, &len) < 0)
        err(1, "recvfrom failed in %s", __PRETTY_FUNCTION__);

    if (len > sizeof(conn.addr))
        errx(1, "The received source address %lu > expected %lu", (size_t) len, sizeof(conn.addr));

    printf("Received packet from %s:%lu\n",
           inet_ntoa(conn.addr.sin_addr),
           (unsigned long) ntohs(conn.addr.sin_port));

    return conn;
}
int output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    const struct Connection *conn = static_cast<struct Connection*>(user);
    if (sendto(conn->sockfd, buf, len, 0, (const struct sockaddr*) &conn->addr, sizeof(conn->addr)) < 0)
        err(1, "sendto failed in %s", __PRETTY_FUNCTION__);
    return 0;
}
bool recv_data(ikcpcb *kcp, const Connection &conn)
{
    constexpr const static auto packet_limit = 100000;

    bool ret = false;
    char buffer[5000];
    if (ikcp_waitsnd(kcp) > packet_limit)
        ikcp_flush(kcp);
    while (true) {
        ssize_t cnt = recv(conn.sockfd, buffer, sizeof(buffer),
                           ikcp_waitsnd(kcp) <= packet_limit ? MSG_DONTWAIT : 0);
        if (cnt < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                err(1, "recv failed in %s", __PRETTY_FUNCTION__);
            break;
        } else {
            ikcp_input(kcp, buffer, cnt);
            ikcp_update(kcp, getUnixTimestamp());
            ret = true;
        }
    }
    return ret;
}
int main(int argc, char* argv[])
{
    struct Connection conn = createConnection();

    puts("Initializing kcp");

    ikcpcb *kcp = ikcp_create(0, &conn);
    ikcp_setoutput(kcp, output);
    ikcp_nodelay(kcp, 1, 20, 2, 1);
    ikcp_wndsize(kcp, 1024, 1024);

    puts("Starts to send data");

    uint64_t byte = 0;
    const auto start = chrono::steady_clock::now();

    char buffer[5000];
    for (; ; byte += sizeof(buffer)) {
        buffer[0] = 0;
        ikcp_send(kcp, buffer, sizeof(buffer));
        ikcp_update(kcp, getUnixTimestamp());

        if (recv_data(kcp, conn)) {
            if (ikcp_recv(kcp, buffer, sizeof(buffer)) > 0) { // receiver wants to end the connection
                ikcp_flush(kcp);
                while (recv_data(kcp, conn)); // ensure all data sent is recevied by receiver
                break;
            }
        }
    }

    const auto end = chrono::steady_clock::now();

    buffer[0] = 1;
    ikcp_send(kcp, buffer, 1);
    ikcp_update(kcp, getUnixTimestamp());
    ikcp_flush(kcp);

    puts("Done sending data\n\n");
    ikcp_release(kcp);

    // byte per seconds
    auto speed = static_cast<double>(byte) / chrono::duration_cast<chrono::seconds>(end - start).count();
    printf("speed = %f MB/s\n", speed / 1000 / 1000);

    return 0;
}
