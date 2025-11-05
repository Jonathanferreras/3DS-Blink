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
#define BUFFER_SIZE 128

static void* socket_buffer = NULL;
static volatile int connected = 0;

PrintConsole top_screen, bottom_screen;


int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &top_screen);
    consoleInit(GFX_BOTTOM, &bottom_screen);

    consoleSelect(&top_screen);
    
    printf("=====================================\n");
    printf("       Welcome to 3DS Blink.           \n");
    printf("=====================================\n\n");  

    socket_buffer = memalign(SOCKET_ALIGN, SOCKET_BUFFER_SIZE);

    if(!socket_buffer || R_FAILED(socInit(socket_buffer, SOCKET_BUFFER_SIZE))) {
        consoleSelect(&bottom_screen);
        printf("Failed to enable socket!\n");
        
        return 0;
    }

    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(tcp_socket < 0) {
        consoleSelect(&bottom_screen);
        printf("Failed to create TCP socket!\n");

        goto cleanup;
    }

    struct sockaddr_in device_address = {
        .sin_family = AF_INET,
        .sin_port = htons(TARGET_PORT)
    };

    if(inet_pton(AF_INET, TARGET_IP, &device_address.sin_addr) <= 0) {
        consoleSelect(&bottom_screen);
        printf("Bad Target IP!\n");

        goto cleanup;
    }

    consoleSelect(&top_screen);
    printf("\nPress X to connect to device.\n");

    while (aptMainLoop()) {
        hidScanInput();

        if (hidKeysDown() & KEY_START) {
            // BUG: Pressing Start is not closing the program, blocking issue with tcp socket.
            goto cleanup;
        }
        else if (hidKeysDown() & KEY_X) {
            if(connected < 1) {
                consoleSelect(&top_screen);
                printf("\nConnecting to device...\n");

                if(connect(tcp_socket, (struct sockaddr*)&device_address, sizeof(device_address)) == -1) {
                    consoleSelect(&bottom_screen);
                    printf("Failed to connect (errno=%d): %s\n", errno, strerror(errno));

                    goto cleanup;
                }

                connected = 1;

                consoleSelect(&top_screen);
                printf("Connected to %s:%d\n", TARGET_IP, TARGET_PORT);

                char message[] = "Hello from 3DS!";
                ssize_t bytes_sent = send(tcp_socket, message, strlen(message), 0);

                if(bytes_sent == -1) {
                    consoleSelect(&bottom_screen);
                    printf("Failed to send message\n");

                    goto cleanup;
                }

                consoleSelect(&top_screen);
                printf("\nPress A to toggle the green LED.\n");
                printf("\nPress B to toggle the red LED.\n");
                printf("\nPress START to exit.\n");
            }
        }
        else {
            const char *payload = NULL;

            if (hidKeysDown() & KEY_A) {
                payload = "0x05";

            } 
            else if (hidKeysDown() & KEY_B) {
                payload = "0x06";
            }

            if(tcp_socket > 0 && payload != NULL) {
                consoleSelect(&top_screen);
                ssize_t payload_sent = send(tcp_socket, payload, strlen(payload), 0);

                if(payload_sent == -1) {
                    consoleSelect(&bottom_screen);
                    printf("Failed to send payload\n");
                }

                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                ssize_t bytes_received = recv(tcp_socket, buffer, BUFFER_SIZE - 1, 0);

                if(bytes_received == -1) {
                    printf("Failed to receive data!");

                    close(tcp_socket);
                } else if (bytes_received == 0) {
                    printf("Device disconnected!");
                } else {
                    buffer[bytes_received] = '\0';

                    consoleSelect(&bottom_screen);
                    printf("Received from device: %s\n", buffer);
                }   
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }


    cleanup:
        close(tcp_socket);
        socExit();
        free(socket_buffer);
        socket_buffer = NULL;
        gfxExit();


    return 0;
}