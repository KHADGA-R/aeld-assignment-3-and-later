#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t stop_requested = 0;

static void signal_handler(int signo)
{
    (void)signo;
    stop_requested = 1;
}

/* Send all requested bytes, handling partial sends. */
static int send_all(int fd, const char *buffer, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(fd, buffer + sent, length - sent, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (result == 0) {
            return -1;
        }

        sent += (size_t)result;
    }

    return 0;
}

/* Stream the file to the client without loading it all into RAM. */
static int send_file(int client_fd)
{
    FILE *file = fopen(DATA_FILE, "rb");

    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int result = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send_all(client_fd, buffer, bytes_read) != 0) {
            syslog(LOG_ERR, "Failed to send file: %s", strerror(errno));
            result = -1;
            break;
        }
    }

    if (ferror(file)) {
        syslog(LOG_ERR, "Failed to read data file");
        result = -1;
    }

    fclose(file);
    return result;
}

/* Append one complete newline-terminated packet. */
static int append_packet(const char *packet, size_t length)
{
    FILE *file = fopen(DATA_FILE, "ab");

    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
        return -1;
    }

    int result = 0;

    if (fwrite(packet, 1, length, file) != length) {
        syslog(LOG_ERR, "Failed to write data file");
        result = -1;
    }

    if (fclose(file) != 0) {
        syslog(LOG_ERR, "Failed to close data file: %s", strerror(errno));
        result = -1;
    }

    return result;
}

/* Receive bytes and process each complete newline-terminated packet. */
static void handle_client(int client_fd)
{
    char buffer[BUFFER_SIZE];
    char *packet = NULL;
    size_t packet_length = 0;
    size_t packet_capacity = 0;

    while (!stop_requested) {
        ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (received == 0) {
            break;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
            break;
        }

        for (ssize_t i = 0; i < received; i++) {
            if (packet_length + 1 > packet_capacity) {
                size_t new_capacity =
                    packet_capacity == 0 ? BUFFER_SIZE : packet_capacity * 2;

                char *new_packet = realloc(packet, new_capacity);

                if (new_packet == NULL) {
                    syslog(LOG_ERR, "Packet allocation failed");
                    free(packet);
                    return;
                }

                packet = new_packet;
                packet_capacity = new_capacity;
            }

            packet[packet_length++] = buffer[i];

            if (buffer[i] == '\n') {
                if (append_packet(packet, packet_length) != 0) {
                    free(packet);
                    return;
                }

                if (send_file(client_fd) != 0) {
                    free(packet);
                    return;
                }

                packet_length = 0;
            }
        }
    }

    free(packet);
}

int main(int argc, char *argv[])
{
    int server_fd;
    struct sockaddr_in server_addr;
    int daemon_mode = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        return -1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        syslog(LOG_ERR, "Failed to install signal handlers");
        closelog();
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        closelog();
        return -1;
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

        /* Run as a daemon if -d was specified.
     * The socket has already been bound to port 9000.
     */
    if (daemon_mode) {
        pid_t pid = fork();

        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }

        if (pid > 0) {
            /* Parent exits; child continues as the server. */
            close(server_fd);
            closelog();
            return 0;
        }

        /* Create a new session for the daemon. */
        if (setsid() == -1) {
            syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }

        /* Disconnect the daemon from the terminal. */
        int devnull = open("/dev/null", O_RDWR);

        if (devnull == -1) {
            syslog(LOG_ERR, "Failed to open /dev/null: %s",
                   strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }

        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);

        if (devnull > STDERR_FILENO) {
            close(devnull);
        }
    }	
    printf("AESD socket server listening on port %d\n", PORT);
    fflush(stdout);

    while (!stop_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &client_addr.sin_addr,
                      client_ip, sizeof(client_ip)) == NULL) {
            syslog(LOG_ERR, "Failed to convert client IP");
            close(client_fd);
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        handle_client(client_fd);

        close(client_fd);

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    syslog(LOG_INFO, "Caught signal, exiting");

    close(server_fd);

    if (unlink(DATA_FILE) == -1 && errno != ENOENT) {
        syslog(LOG_ERR, "Failed to delete data file: %s",
               strerror(errno));
    }

    closelog();

    return 0;
}
