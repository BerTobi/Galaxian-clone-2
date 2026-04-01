#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")

int main(void) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(2112);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        return 1;
    }
    if (listen(s, 5) == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        return 1;
    }

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET conn = accept(s, (struct sockaddr*)&client_addr, &client_len);
        if (conn == INVALID_SOCKET) {
            printf("accept failed: %d\n", WSAGetLastError());
            break;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        printf("new connection from %s:%d\n", ip_str, ntohs(client_addr.sin_port));

        char buf[1024];
        int n;
        while ((n = recv(conn, buf, sizeof(buf), 0)) > 0) {
            buf[n] = '\0';
            printf("> %s\n", buf);
            send(conn, buf, n, 0);
        }

        printf("no more data, dropping connection\n");
        closesocket(conn);
    }
}