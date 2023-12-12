#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_CLIENTS 1

int server_sock = -1;
int client_socket = -1;
volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    if (sig == SIGHUP) {
        printf("Получен сигнал SIGHUP. Завершение\n");
        running = 0;
    }
}

int main() {
    struct sockaddr_in server_addr;
    int new_socket;

    // Обработка сигналов
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGHUP, &sa, NULL);

    // Создание серверного сокета
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345);

    // Привязка сокета
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Ожидание входящих соединений
    if (listen(server_sock, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    sigset_t blockedMask;
    sigset_t origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        int max_fd = server_sock;

        if (client_socket != -1) {
            FD_SET(client_socket, &read_fds);
            max_fd = (client_socket > max_fd) ? client_socket : max_fd;
        }

        struct timespec timeout = { 1, 0 };  // Ожидание событий ввода-вывода 1 секунда

        int activity = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, &origMask);

        if (activity < 0) {
            if (errno == EINTR) {
                continue;  // Продолжаем, если сигнал был обработан
            } else {
                perror("pselect");
                break;
            }
        }

        if (activity == 0) {
            // Таймаут, ничего не произошло
            continue;
        }

        if (FD_ISSET(server_sock, &read_fds)) {
            if ((new_socket = accept(server_sock, NULL, NULL)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            if (client_socket != -1) {
                close(new_socket);
            } else {
                printf("Принято новое соединение\n");
                client_socket = new_socket;
            }
        }

        if (client_socket != -1 && FD_ISSET(client_socket, &read_fds)) {
            char buffer[1024];
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

            if (bytes_received > 0) {
                printf("Получены данные от клиента: %ld байт\n", bytes_received);
            } else if (bytes_received == 0) {
                printf("Клиент отключен\n");
                close(client_socket);
                client_socket = -1;
            } else {
                perror("recv");
            }
        }
    }

    // Закрытие сокетов перед выходом
    close(server_sock);
    close(client_socket);

    return 0;
}

