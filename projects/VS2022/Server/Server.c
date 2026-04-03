#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")

int main(void) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(2112);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) 
    {
        printf("bind failed: %d\n", WSAGetLastError());
        return 1;
    }
    else
    {
        printf("UDP echo server listening on port 2112...\n");
    }

    char buf[1024];
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    while (1) {
        int n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &client_len);
        if (n <= 0) continue;
        buf[n] = '\0';
        printf("> %s\n", buf);
        sendto(s, buf, n, 0, (struct sockaddr *)&client_addr, client_len);
    }
}