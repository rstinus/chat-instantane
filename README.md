[![Licence : CC BY-NC 4.0](https://img.shields.io/badge/Licence-CC%20BY--NC%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc/4.0/)

# Chat Instantané

Chat instantané pour terminal Linux, avec des commandes serveur et un fichier de log.
Le serveur ne récupère que l'IP et le nom d'utilisateur du client qui souhaite se connecter.
Si le serveur redémarre, les informations sur les bans seront conservées dans `ban.log`.

---

## Description

Projet de chat client/serveur en C utilisant les sockets TCP.

### Fonctionnalités principales :

* Multi-client avec identification par pseudo
* Gestion des pseudos déjà pris (empêche la duplication)
* Commandes serveur : `kick`, `ban`, `unban`, `list`, `showbans`, `help`, `quit`
* Gestion des IP bannies avec notification `BANNED\n` au client
* Historique des bans dans `ban.log`

### Caractéristiques :

* Choix du port utilisé pour le serveur
* Connexion avec IP locale ou distante via le client
* Limite de 100 utilisateurs simultanés
* Limite de 100 IP bannies simultanées

---

## Compilation

Assurez-vous d’avoir un compilateur C installé (gcc, clang…).

```bash
gcc -Wall -O2 -o server server.c
gcc -Wall -O2 -o client client.c
```

---

## Usage

### Lancer le serveur

```bash
./server <port>
```

Exemple :

```bash
./server 12345
```

### Lancer le client

```bash
./client <ip_serveur> <port> [pseudo]
```

Exemple :

```bash
./client 127.0.0.1 12345 Alice
```

---

## Commandes serveur

* `kick <username>` : Expulse un utilisateur
* `ban <ip>` : Bannit une IP et déconnecte les clients concernés
* `unban <ip>` : Débannit une IP
* `showbans` : Affiche la liste des IP bannies
* `list` : Liste les clients connectés
* `help` : Affiche l’aide
* `quit` : Arrête le serveur

---

## Fichiers générés / importants

* `server.c` : Code source du serveur
* `client.c` : Code source du client
* `ban.log` : Historique des bans (créé automatiquement par le serveur)
* `LICENSE` : Licence du projet (CC BY-NC 4.0)

---

## Licence

Ce projet est sous licence **Creative Commons Attribution - Pas d’Utilisation Commerciale 4.0 International (CC BY-NC 4.0)**.
Vous pouvez partager et modifier le code, à condition de **créditer l’auteur** et de **ne pas l’utiliser commercialement**.

Lien officiel : [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/)

---
