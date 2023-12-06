#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>

#define MAX_CLIENTS 1

int server_sock = -1;
int client_socket = -1;
pthread_mutex_t mutex;
volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    if (sig == SIGHUP) {
        printf("Получен сигнал SIGHUP. Завершение\n");
        running = 0;
    }
}

void* connection_handler(void* socket_desc) {
    int client_sock = *(int*)socket_desc;
    char buffer[1024];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
        pthread_mutex_lock(&mutex);
        printf("Получены данные от клиента: %ld байт\n", bytes_received);
        pthread_mutex_unlock(&mutex);
    }

    if (bytes_received == 0) {
        pthread_mutex_lock(&mutex);
        printf("Клиент отключен\n");
        pthread_mutex_unlock(&mutex);
    } else {
        perror("recv");
    }

    close(client_sock);
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    int new_socket;

    // Обработка сигналов
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGHUP, &sa, NULL);

    // Инициализация мьютекса
    pthread_mutex_init(&mutex, NULL);

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

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_sock, &read_fds);

    while (running) {
        int max_fd = server_sock;

        // Добавление клиентского сокета в набор, если подключен
        if (client_socket != -1) {
            FD_SET(client_socket, &read_fds);
            max_fd = (client_socket > max_fd) ? client_socket : max_fd;
        }

        // Ожидание активности на одном из сокетов или сигналах
        pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, NULL);

        // Проверка входящего соединения
        if (FD_ISSET(server_sock, &read_fds)) {
            if ((new_socket = accept(server_sock, NULL, NULL)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // Закрытие дополнительных соединений
            if (client_socket != -1) {
                close(new_socket);
            } else {
                pthread_mutex_lock(&mutex);
                printf("Принято новое соединение\n");
                pthread_mutex_unlock(&mutex);

                client_socket = new_socket;

                // Создание потока для обработки клиентского соединения
                pthread_t thread;
                if (pthread_create(&thread, NULL, connection_handler, (void*)&client_socket) < 0) {
                    perror("pthread_create");
                    exit(EXIT_FAILURE);
                }
                pthread_detach(thread);
            }
        }

        // Очистка набора для следующей итерации
        FD_ZERO(&read_fds);
    }

    // Закрытие сокетов и уничтожение мьютекса перед выходом
    close(server_sock);
    close(client_socket);
    pthread_mutex_destroy(&mutex);

    return 0;
}
