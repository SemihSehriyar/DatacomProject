// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Mesajları almak için bir iş parçacığı oluştur
void *receive_messages(void *socket) {
    int sock = *(int *)socket;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    printf("Sunucuyla bağlantı kesildi.\n");
    exit(0);
}

int main() {
    int sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];
    char name[BUFFER_SIZE];

    // Kullanıcıdan isim al
    printf("Adınızı girin: ");
    fgets(name, BUFFER_SIZE, stdin);
    name[strcspn(name, "\n")] = 0; // Yeni satır karakterini kaldır

    // Soket oluştur
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Soket oluşturma hatası");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // IPv4 adresini metin formattan ikili forma dönüştür
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("Geçersiz adres/Adres desteklenmiyor");
        return -1;
    }

    // Sunucuya bağlan
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bağlantı hatası");
        return -1;
    }

    // Kullanıcı adını sunucuya gönder
    send(sock, name, strlen(name), 0);

    // Mesajları almak için bir iş parçacığı başlat
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, &sock) != 0) {
        perror("İş parçacığı oluşturma hatası");
        return -1;
    }

    // Mesajları sunucuya gönder
    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Yeni satır karakterini kaldır
        send(sock, buffer, strlen(buffer), 0);
    }

    close(sock);
    return 0;
}