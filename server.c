// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

// İstemciler için global değişkenler
int clients[MAX_CLIENTS];
char client_names[MAX_CLIENTS][BUFFER_SIZE];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Belirli bir istemciye mesaj gönder
void send_private_message(const char *target_name, const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(client_names[i], target_name) == 0) {
            send(clients[i], message, strlen(message), 0);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // Hedef kullanıcı bulunamazsa gönderen istemciye hata mesajı
    char error_message[BUFFER_SIZE];
    snprintf(error_message, sizeof(error_message), "Kullanıcı %s bulunamadı.\n", target_name);
    send(sender_socket, error_message, strlen(error_message), 0);
}

// Mesajı tüm istemcilere ilet
void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] != sender_socket) {
            send(clients[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Her bir istemciyi yönet
void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    free(client_socket);
    char buffer[BUFFER_SIZE];
    char client_name[BUFFER_SIZE];

    // İstemci adını al
    int bytes_read = recv(sock, client_name, BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        close(sock);
        return NULL;
    }
    client_name[bytes_read] = '\0';

    // İstemci adını kaydet
    pthread_mutex_lock(&clients_mutex);
    clients[client_count] = sock;
    strcpy(client_names[client_count], client_name);
    client_count++;
    pthread_mutex_unlock(&clients_mutex);

    // Diğer istemcilere yeni bağlantıyı bildir
    char join_message[BUFFER_SIZE];
    snprintf(join_message, sizeof(join_message), "%s sohbete giris yapti.\n", client_name);
    printf("%s", join_message);
    broadcast_message(join_message, sock);

    // İletişim döngüsü
    while ((bytes_read = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_read] = '\0'; // String sonuna null karakter ekle

        // Özel mesaj kontrolü
        if (buffer[0] == '@') {
            char target_name[BUFFER_SIZE];
            char *message_body = strchr(buffer, ':');
            if (message_body != NULL) {
                *message_body = '\0'; // Kullanıcı adı ve mesajı ayır
                message_body++;
                strcpy(target_name, buffer + 1); // '@' işaretini atla

                // Özel mesaj formatı: "[Gönderen]: mesaj"
                char private_message[BUFFER_SIZE];
                snprintf(private_message, sizeof(private_message), "[Özel %s]: %s\n", client_name, message_body);
                send_private_message(target_name, private_message, sock);
            } else {
                char error_message[] = "Özel mesaj formatı hatalı. Kullanım: @kullanici_adi: mesaj\n";
                send(sock, error_message, strlen(error_message), 0);
            }
        } else {
            // Genel mesaj formatı
            printf("%s: %s\n", client_name, buffer);

            char message[BUFFER_SIZE + 50];
            snprintf(message, sizeof(message), "%s: %s\n", client_name, buffer);
            broadcast_message(message, sock);
        }
    }

    // İstemci bağlantısı kesildi
    snprintf(join_message, sizeof(join_message), "%s sohbetten ayrildi.\n", client_name);
    printf("%s", join_message);
    broadcast_message(join_message, sock);

    // İstemciyi global listeden çıkar
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] == sock) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
                strcpy(client_names[j], client_names[j + 1]);
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Soket oluştur
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Soket Hatasi");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Soketi porta bağla
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Bağlantıları dinle
    if (listen(server_fd, 5) < 0) {
        perror("Dinleme Hatasi");
        exit(EXIT_FAILURE);
    }

    printf("Server %d portunda dinliyor...\n", PORT);

    while (1) {
        // Yeni bağlantıları kabul et
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Yeni bir istemci bağlandı.\n");

        // Her istemci için yeni bir iş parçacığı oluştur
        pthread_t thread_id;
        int *client_sock = malloc(sizeof(int));
        *client_sock = new_socket;
        if (pthread_create(&thread_id, NULL, handle_client, client_sock) != 0) {
            perror("İş parçacığı oluşturma hatası");
        }

        pthread_detach(thread_id); // Kaynakları otomatik olarak temizlemek için iş parçacığını ayır
    }

    return 0;
}