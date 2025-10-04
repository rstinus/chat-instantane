#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFERSIZE 1024

int sock = -1;

void handle_sigint(int sig) {
    (void)sig;
    if (sock >= 0) close(sock);
    puts("\nConnexion au serveur terminée");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> [username]\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *username = (argc >= 4) ? argv[3] : "You";

    struct sockaddr_in servaddr;
    char buf[BUFFERSIZE];
    signal(SIGINT, handle_sigint);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);
    servaddr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    dprintf(sock, "%s\n", username);
    printf("Client connecté en tant que '%s'\n", username);
    printf("%s> ", username);
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(sock);
        return 1;
    }

    if (pid == 0) {
        while (1) {
            ssize_t n = read(sock, buf, BUFFERSIZE - 1);
            if (n <= 0) break;
            buf[n] = '\0';

            if (strncmp(buf, "BANNED", 6) == 0) {
                printf("\nVous avez été banni du serveur.\n");
                close(sock);
                _exit(0);
            }

            printf("\n%s%s> ", buf, username);
            fflush(stdout);
        }
        close(sock);
        _exit(0);
    } else {
        while (fgets(buf, BUFFERSIZE, stdin)) {
            if (write(sock, buf, strlen(buf)) < 0) {
                puts("\nConnexion perdue");
                break;
            }
        }
        shutdown(sock, SHUT_WR);
        close(sock);
    }

    return 0;
}
