#include <windows.h>
#include <stdio.h>
#include <time.h>

HANDLE fileMutex;
const int maxClients = 10;
const int bufferSize = 256;
const wchar_t pipeName[] = L"\\\\.\\pipe\\ServerPipe";

void writeToLog(const char* client, const char* message) {
    WaitForSingleObject(fileMutex, INFINITE);

    HANDLE file = CreateFile(L"log.txt", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file != INVALID_HANDLE_VALUE) {
        char messageExt[bufferSize + 60];
        char timeStr[20];
        struct tm timeinfo;
        time_t now = time(NULL);
        localtime_s(&timeinfo, &now);
        strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);

        snprintf(messageExt, sizeof(messageExt), "%s Client ID: #%s Message: %s\n", timeStr, client, message);

        WriteFile(file, messageExt, strlen(messageExt), NULL, NULL);
        CloseHandle(file);
    }
    ReleaseMutex(fileMutex);
}


DWORD WINAPI clientThreadHandler(LPVOID param) {
    HANDLE pipe = (HANDLE)param;
    char buffer[bufferSize];
    DWORD bytesRead;
    char client[10];
    snprintf(client, sizeof(client), "%d", GetCurrentThreadId());

    while (ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        buffer[bytesRead] = '\0';
        writeToLog(client, buffer);
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

int main() {
    HANDLE pipe;
    HANDLE threads[maxClients];
    int threadCount = 0;

    HANDLE hFile = CreateFile(L"log.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    CloseHandle(hFile);

    fileMutex = CreateMutex(NULL, FALSE, NULL);

    if (fileMutex == NULL) return 1;

    while (true) {
        pipe = CreateNamedPipe(pipeName, PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, maxClients, bufferSize, bufferSize, 0, NULL);

        if (pipe == INVALID_HANDLE_VALUE) return 1;

        printf("Pipe created, awaiting clients\n");

        BOOL connected = ConnectNamedPipe(pipe, NULL);

        if (connected) {
            printf("Client connected\n");
            if (threadCount < maxClients) {
                threads[threadCount] = CreateThread(NULL, 0, clientThreadHandler, (LPVOID)pipe, 0, NULL);
                threadCount++;
                if (threads[threadCount] == NULL) {
                    printf("CreateThread failed, Error: %ld\n", GetLastError());
                    CloseHandle(pipe);
                }
            }
            else {
                printf("Max clients reached. Connection refused.\n");
                DisconnectNamedPipe(pipe);
                CloseHandle(pipe);
            }
        }
    }

    WaitForMultipleObjects(threadCount, threads, TRUE, INFINITE);

    for (int i = 0; i < threadCount; i++) {
        CloseHandle(threads[i]);
    }

    CloseHandle(fileMutex);
    return 0;
}