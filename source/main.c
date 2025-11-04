#include <3ds.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"

#define SOCKET_ALIGN       0x1000
#define SOCKET_BUFFER_SIZE (1 * 1024 * 1024)
static void* socket_buffer = NULL;

PrintConsole top_screen, bottom_screen;


int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &top_screen);
    consoleInit(GFX_BOTTOM, &bottom_screen);

    consoleSelect(&top_screen);
    
    printf("=====================================\n");
    printf("       Welcome to 3DS Blink.           \n");
    printf("=====================================\n\n");  

    // 1. Enable socket
    socket_buffer = memalign(SOCKET_ALIGN, SOCKET_BUFFER_SIZE);

    if(!socket_buffer || R_FAILED(socInit(socket_buffer, SOCKET_BUFFER_SIZE))) {
        consoleSelect(&bottom_screen);
        printf("Failed to enable socket!\n");
        
        goto wait_exit;
    }

    // 2. Create TCP Socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(tcp_socket < 0) {
        consoleSelect(&bottom_screen);
        printf("Failed to create TCP socket!\n");

        goto cleanup;
    }

    // 3. Construct IP Address
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(TARGET_PORT) // htons converts port to short (2 bytes)
    };

    if(inet_pton(AF_INET, TARGET_IP, &server_address.sin_addr) <= 0) {
        consoleSelect(&bottom_screen);
        printf("Bad Target IP!\n");

        goto cleanup;
    }

    // 4. Connect
    consoleSelect(&top_screen);
    printf("Connecting to socket...\n");

    if(connect(tcp_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        consoleSelect(&bottom_screen);
        printf("Failed to connect (errno=%d): %s\n", errno, strerror(errno));

        goto cleanup;
    }

    consoleSelect(&top_screen);
    printf("Connected to %s:%d\n", TARGET_IP, TARGET_PORT);


    char message[] = "Hello from 3DS!";

    ssize_t bytes_sent = send(tcp_socket, message, strlen(message), 0);

    if(bytes_sent == -1) {
        consoleSelect(&bottom_screen);
        printf("Failed to send message\n");

        goto cleanup;
    } else {
        goto wait_exit;
    }

    cleanup:
        close(tcp_socket);
        socExit();
        free(socket_buffer);
        socket_buffer = NULL;

        goto wait_exit;

    wait_exit:
        consoleSelect(&top_screen);

        printf("\nPress START to exit.\n");

        while (aptMainLoop()) {
            hidScanInput();

            if (hidKeysDown() & KEY_START) {
                break;
            }

            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        }

        gfxExit();

        return 0;
}