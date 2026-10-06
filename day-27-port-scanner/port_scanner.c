/* port_scanner.c
 *
 * A simple TCP port scanner for localhost (127.0.0.1).
 *
 * WARNING: Only scan systems you own or have explicit permission
 * to scan. Scanning networks you don't own is illegal in many
 * jurisdictions.
 *
 * This program is for educational purposes only and should
 * only be used on your own machine.
 *
 * Usage:
 *   ./port_scanner <start_port> <end_port>
 *
 * Example:
 *   ./port_scanner 1 1024
 *
 * Skills practiced:
 *   - Sockets (socket, connect, close)
 *   - Networking (TCP, ports)
 *   - System calls
 *   - Security concepts (port scanning)
 *   - argc / argv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LOCALHOST "127.0.0.1"
#define TIMEOUT_SEC 1

/* Try to connect to a specific port on a host.
 * Returns 1 if open, 0 if closed.
 */
int scan_port(const char *host, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return 0;

    /* Set a receive timeout so connect doesn't hang forever */
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    int result = connect(sock, (struct sockaddr *) &addr, sizeof(addr));
    close(sock);

    return (result == 0) ? 1 : 0;
}

/* Return a short name for well-known ports */
const char *port_name(int port)
{
    switch (port)
    {
        case 20:   return "FTP-data";
        case 21:   return "FTP";
        case 22:   return "SSH";
        case 23:   return "Telnet";
        case 25:   return "SMTP";
        case 53:   return "DNS";
        case 80:   return "HTTP";
        case 110:  return "POP3";
        case 143:  return "IMAP";
        case 443:  return "HTTPS";
        case 445:  return "SMB";
        case 3306: return "MySQL";
        case 5432: return "PostgreSQL";
        case 6379: return "Redis";
        case 8080: return "HTTP-alt";
        default:   return NULL;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <start_port> <end_port>\n", argv[0]);
        return 1;
    }

    int start = atoi(argv[1]);
    int end = atoi(argv[2]);

    if (start < 1 || end > 65535 || start > end)
    {
        fprintf(stderr, "Error: ports must be 1–65535 and start <= end.\n");
        return 1;
    }

    printf("====================================\n");
    printf("         PORT SCANNER               \n");
    printf("====================================\n\n");
    printf("Target: %s\n", LOCALHOST);
    printf("Range:  %d–%d\n\n", start, end);
    printf("⚠️  Only scan systems you own.\n\n");

    int open_count = 0;

    for (int port = start; port <= end; port++)
    {
        if (scan_port(LOCALHOST, port))
        {
            const char *name = port_name(port);
            printf("Port %5d: OPEN", port);
            if (name != NULL)
                printf("  (%s)", name);
            printf("\n");
            open_count++;
        }
    }

    printf("\nScan complete. %d open port(s) found.\n", open_count);
    return 0;
}
