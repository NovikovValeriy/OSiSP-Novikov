#include <windows.h>
#include <stdio.h>

const int bufferSize = 256;
const wchar_t pipeName[] = L"\\\\.\\pipe\\ServerPipe";

int main() {
    HANDLE pipeHandle;
    char buffer[bufferSize];
    while (true) {
        pipeHandle = CreateFile(pipeName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipeHandle != INVALID_HANDLE_VALUE) break;
        Sleep(1000);
    }
    while (true) {
        printf("Enter message: ");
        fgets(buffer, bufferSize, stdin);
        int i = strcspn(buffer, "\n");
        buffer[i] = '\0';
        bool wroteToFile = WriteFile(pipeHandle, buffer, strlen(buffer) + 1, NULL, NULL);
        if (!wroteToFile) break;
    }
    CloseHandle(pipeHandle);
    return 0;
}