/*
 * Copyright (c) 2025 Rémy Stinus
 *
 * Ce projet est sous licence Creative Commons Attribution - Pas d’Utilisation Commerciale 4.0 International (CC BY-NC 4.0)
 * https://creativecommons.org/licenses/by-nc/4.0/
 *
 * Vous pouvez partager et modifier ce code, à condition de :
 *   - créditer l’auteur original,
 *   - ne pas en faire un usage commercial.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define BUFFERSIZE 1024
#define BACKLOG 10
#define MAX_CLIENTS 100
#define NAME_LEN 20
#define MAX_BANS 100

typedef struct {
    int fd;
    char username[NAME_LEN];
    int has_name;
    char ip_address[INET_ADDRSTRLEN];
} Client;

Client clients[MAX_CLIENTS];
int nb_clients = 0;

char banned_ips[MAX_BANS][INET_ADDRSTRLEN];
int nb_bans = 0;

int listenfd = -1;
volatile sig_atomic_t stop_server = 0;
FILE *ban_log = NULL;

void handle_sigint(int sig) {
    (void)sig;
    stop_server = 1;
}

void log_ban_action(const char *ip, const char *action) {
    if (!ban_log) return;
    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strcspn(ts, "\n")] = 0; // Supprime le saut de ligne
    fprintf(ban_log, "[%s] %s: %s\n", ts, action, ip);
    fflush(ban_log);
}

void broadcast(int sender_fd, const char *msg, size_t len) {
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].fd != sender_fd) {
            write(clients[i].fd, msg, len);
        }
    }
}

int is_ip_banned(const char *ip) {
    for (int i = 0; i < nb_bans; i++) {
        if (strcmp(banned_ips[i], ip) == 0)
            return 1;
    }
    return 0;
}

void remove_client(int i) {
    close(clients[i].fd);
    for (int j = i; j < nb_clients - 1; j++) {
        clients[j] = clients[j + 1];
    }
    nb_clients--;
}

int find_client_by_name(const char *name) {
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].has_name && strcmp(clients[i].username, name) == 0)
            return i;
    }
    return -1;
}

void kick_user(const char *name) {
    int i = find_client_by_name(name);
    if (i == -1) {
        printf("Utilisateur '%s' introuvable\n", name);
        return;
    }

    // Message envoyé au client expulsé
    const char *kick_msg = "Vous avez été expulsé du serveur.\n";
    write(clients[i].fd, kick_msg, strlen(kick_msg));

    // Message diffusé aux autres utilisateurs
    char msg[128];
    snprintf(msg, sizeof(msg), "%s a été expulsé du serveur.\n", clients[i].username);
    broadcast(clients[i].fd, msg, strlen(msg));

    printf("Utilisateur '%s' (fd=%d) expulsé.\n", clients[i].username, clients[i].fd);

    remove_client(i);
}

// 🔹 Fonction pour recharger les bans depuis ban.log
void load_bans_from_file() {
    FILE *f = fopen("ban.log", "r");
    if (!f) {
        printf("Aucun fichier ban.log trouvé, aucun ban chargé.\n");
        return;
    }

    char line[256];
    char ip[INET_ADDRSTRLEN];
    char action[16];

    nb_bans = 0; // On repart d'une liste vide
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "[%*[^]]] %15[^:]: %15s", action, ip) == 2) {
            if (strcmp(action, "BAN") == 0) {
                // Ajouter l'IP si pas déjà bannie
                int already_banned = 0;
                for (int i = 0; i < nb_bans; i++) {
                    if (strcmp(banned_ips[i], ip) == 0) {
                        already_banned = 1;
                        break;
                    }
                }
                if (!already_banned && nb_bans < MAX_BANS) {
                    strncpy(banned_ips[nb_bans], ip, INET_ADDRSTRLEN - 1);
                    banned_ips[nb_bans][INET_ADDRSTRLEN - 1] = '\0';
                    nb_bans++;
                }
            } else if (strcmp(action, "UNBAN") == 0) {
                // Supprimer l'IP si elle existe
                for (int i = 0; i < nb_bans; i++) {
                    if (strcmp(banned_ips[i], ip) == 0) {
                        for (int j = i; j < nb_bans - 1; j++)
                            strcpy(banned_ips[j], banned_ips[j + 1]);
                        nb_bans--;
                        break;
                    }
                }
            }
        }
    }

    fclose(f);
    printf("%d IP(s) activement bannie(s) chargée(s) depuis ban.log.\n", nb_bans);
}

void ban_ip(const char *ip) {
    if (is_ip_banned(ip)) {
        printf("IP %s est déjà bannie.\n", ip);
        return;
    }

    strncpy(banned_ips[nb_bans], ip, INET_ADDRSTRLEN - 1);
    banned_ips[nb_bans][INET_ADDRSTRLEN - 1] = '\0';
    nb_bans++;

    printf("IP %s bannie.\n", ip);
    log_ban_action(ip, "BAN");

    // Expulsion immédiate si déjà connecté
    for (int i = 0; i < nb_clients;) {
        if (strcmp(clients[i].ip_address, ip) == 0) {
            const char *kick_msg = "BANNED\n";
            write(clients[i].fd, kick_msg, strlen(kick_msg));

            char msg[128];
            snprintf(msg, sizeof(msg), "%s a été banni et expulsé.\n",
                     clients[i].has_name ? clients[i].username : clients[i].ip_address);
            broadcast(clients[i].fd, msg, strlen(msg));

            printf("Déconnexion de fd=%d (IP %s)\n", clients[i].fd, ip);
            remove_client(i);
        } else {
            i++;
        }
    }
}

void unban_ip(const char *ip) {
    for (int i = 0; i < nb_bans; i++) {
        if (strcmp(banned_ips[i], ip) == 0) {
            for (int j = i; j < nb_bans - 1; j++)
                strcpy(banned_ips[j], banned_ips[j + 1]);
            nb_bans--;
            printf("IP %s débannie.\n", ip);
            log_ban_action(ip, "UNBAN");
            return;
        }
    }
    printf("IP %s non trouvée dans la liste des bannis.\n", ip);
}

void show_bans() {
    if (nb_bans == 0) {
        printf("Aucune IP bannie actuellement.\n");
        return;
    }

    printf("Liste des IP bannies (%d):\n", nb_bans);
    for (int i = 0; i < nb_bans; i++) {
        printf("  [%d] %s\n", i + 1, banned_ips[i]);
    }
}

void list_clients() {
    printf("Clients connectés (%d):\n", nb_clients);
    for (int i = 0; i < nb_clients; i++) {
        printf("  [%d] %s (%s)\n", clients[i].fd,
               clients[i].has_name ? clients[i].username : "(non identifié)",
               clients[i].ip_address);
    }
}

void print_help() {
    puts("Commandes disponibles :");
    puts("  kick <username>   - Expulse un utilisateur");
    puts("  ban <ip>          - Bannit une IP et déconnecte le client correspondant");
    puts("  unban <ip>        - Débannit une IP");
    puts("  showbans          - Affiche la liste des IP bannies");
    puts("  list              - Liste les clients connectés");
    puts("  help              - Affiche cette aide");
    puts("  quit              - Arrête le serveur");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen;
    char buf[BUFFERSIZE];

    signal(SIGINT, handle_sigint);

    // Chargement des IP bannies avant d'ouvrir le log
    load_bans_from_file();
    ban_log = fopen("ban.log", "a");
    if (!ban_log) perror("Erreur ouverture ban.log");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ||
        listen(listenfd, BACKLOG) < 0) {
        perror("bind/listen");
        close(listenfd);
        return 1;
    }

    printf("Serveur lancé sur le port %d\n", port);
    print_help();

    while (!stop_server) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listenfd, &fds);
        FD_SET(STDIN_FILENO, &fds);
        int maxfd = listenfd > STDIN_FILENO ? listenfd : STDIN_FILENO;

        for (int i = 0; i < nb_clients; i++) {
            FD_SET(clients[i].fd, &fds);
            if (clients[i].fd > maxfd) maxfd = clients[i].fd;
        }

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) continue;

        // Commandes serveur
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            char cmd[128];
            if (!fgets(cmd, sizeof(cmd), stdin)) break;
            cmd[strcspn(cmd, "\r\n")] = 0;

            if (strncmp(cmd, "kick ", 5) == 0)
                kick_user(cmd + 5);
            else if (strncmp(cmd, "ban ", 4) == 0)
                ban_ip(cmd + 4);
            else if (strncmp(cmd, "unban ", 6) == 0)
                unban_ip(cmd + 6);
            else if (strcmp(cmd, "showbans") == 0)
                show_bans();
            else if (strcmp(cmd, "list") == 0)
                list_clients();
            else if (strcmp(cmd, "help") == 0)
                print_help();
            else if (strcmp(cmd, "quit") == 0)
                break;
            else
                puts("Commande inconnue. Tape 'help' pour la liste.");
        }

        // Nouvelle connexion
        if (FD_ISSET(listenfd, &fds)) {
            clilen = sizeof(cliaddr);
            int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
            if (connfd < 0) continue;

            const char *ip = inet_ntoa(cliaddr.sin_addr);
            if (is_ip_banned(ip)) {
                write(connfd, "BANNED\n", 7);
                log_ban_action(ip, "CONN_REFUSED");
                close(connfd);
                continue;
            }

            if (nb_clients >= MAX_CLIENTS) {
                close(connfd);
                continue;
            }

            clients[nb_clients].fd = connfd;
            clients[nb_clients].has_name = 0;
            strncpy(clients[nb_clients].ip_address, ip, INET_ADDRSTRLEN - 1);
            clients[nb_clients].ip_address[INET_ADDRSTRLEN - 1] = '\0';
            nb_clients++;
            printf("Connexion de %s (fd=%d)\n", ip, connfd);
        }

        // Messages clients
        for (int i = 0; i < nb_clients; i++) {
            int fd = clients[i].fd;
            if (FD_ISSET(fd, &fds)) {
                ssize_t n = read(fd, buf, BUFFERSIZE - 1);
                if (n <= 0) {
                    if (clients[i].has_name) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "%s s'est déconnecté.\n", clients[i].username);
                        broadcast(fd, msg, strlen(msg));
                    }
                    remove_client(i--);
                    continue;
                }
                buf[n] = '\0';

                if (!clients[i].has_name) {
                    // Nettoyer le nom reçu
                    strncpy(clients[i].username, buf, NAME_LEN - 1);
                    clients[i].username[strcspn(clients[i].username, "\r\n")] = 0;

                    // Vérifier si le nom est déjà pris
                    int name_taken = 0;
                    for (int j = 0; j < nb_clients; j++) {
                        if (i != j && clients[j].has_name && strcmp(clients[j].username, clients[i].username) == 0) {
                            name_taken = 1;
                            break;
                        }
                    }

                    if (name_taken) {
                        const char *msg = "ERREUR: Ce nom d'utilisateur est déjà pris. Veuillez vous reconnecter avec un autre.\n";
                        write(fd, msg, strlen(msg));
                        printf("Connexion refusée : nom '%s' déjà pris.\n", clients[i].username);
                        remove_client(i--); // Retire immédiatement le client
                        continue;
                    }

                    // Si le nom est disponible
                    clients[i].has_name = 1;
                    printf("fd=%d identifié comme '%s'\n", fd, clients[i].username);

                    char msg[128];
                    snprintf(msg, sizeof(msg), "%s a rejoint le chat.\n", clients[i].username);
                    broadcast(fd, msg, strlen(msg));
                }
                else {
                    char msg[BUFFERSIZE + NAME_LEN + 4];
                    snprintf(msg, sizeof(msg), "%s > %s", clients[i].username, buf);
                    broadcast(fd, msg, strlen(msg));
                }
            }
        }
    }

    for (int i = 0; i < nb_clients; i++) close(clients[i].fd);
    if (listenfd >= 0) close(listenfd);
    if (ban_log) fclose(ban_log);
    puts("Serveur arrêté.");

    return 0;
}
