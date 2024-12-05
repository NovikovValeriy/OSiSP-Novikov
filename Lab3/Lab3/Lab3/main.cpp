#include <windows.h>
#include <iostream>
#include <string>

const int BUFFER_SIZE = 256;
const int NUM_BUFFERS = 4;

struct SharedBuffer {
    char data[BUFFER_SIZE];
    bool isOccupied;
};

struct SharedMemory {
    SharedBuffer buffers[NUM_BUFFERS];
};

const char* SHARED_MEMORY_NAME = "Global\\SharedMemoryExample";
const char* MUTEX_NAME = "Global\\SharedMemoryMutex";

void WriterProcess() {
    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_WRITE, FALSE, SHARED_MEMORY_NAME);
    if (!hMapFile) {
        std::cerr << "Unable to open shared memory. Error: " << GetLastError() << "\n";
        return;
    }

    SharedMemory* sharedMem = (SharedMemory*)MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, sizeof(SharedMemory));
    if (!sharedMem) {
        std::cerr << "Unable to map view of file. Error: " << GetLastError() << "\n";
        CloseHandle(hMapFile);
        return;
    }

    HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
    if (!hMutex) {
        std::cerr << "Unable to open mutex. Error: " << GetLastError() << "\n";
        UnmapViewOfFile(sharedMem);
        CloseHandle(hMapFile);
        return;
    }

    for (int i = 0; i < 10; ++i) {
        WaitForSingleObject(hMutex, INFINITE);

        for (int j = 0; j < NUM_BUFFERS; ++j) {
            if (!sharedMem->buffers[j].isOccupied) {
                std::string message = "Message " + std::to_string(i);
                strncpy_s(sharedMem->buffers[j].data, message.c_str(), BUFFER_SIZE - 1);
                sharedMem->buffers[j].isOccupied = true;
                std::cout << "Writer: Wrote \"" << message << "\" to buffer " << j << "\n";
                break;
            }
        }

        ReleaseMutex(hMutex);
        Sleep(1000);
    }

    UnmapViewOfFile(sharedMem);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);
}

void ReaderProcess() {
    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, SHARED_MEMORY_NAME);
    if (!hMapFile) {
        std::cerr << "Unable to open shared memory. Error: " << GetLastError() << "\n";
        return;
    }

    SharedMemory* sharedMem = (SharedMemory*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(SharedMemory));
    if (!sharedMem) {
        std::cerr << "Unable to map view of file. Error: " << GetLastError() << "\n";
        CloseHandle(hMapFile);
        return;
    }

    HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
    if (!hMutex) {
        std::cerr << "Unable to open mutex. Error: " << GetLastError() << "\n";
        UnmapViewOfFile(sharedMem);
        CloseHandle(hMapFile);
        return;
    }

    for (int i = 0; i < 10; ++i) {
        WaitForSingleObject(hMutex, INFINITE);

        for (int j = 0; j < NUM_BUFFERS; ++j) {
            if (sharedMem->buffers[j].isOccupied) {
                std::cout << "Reader: Read \"" << sharedMem->buffers[j].data << "\" from buffer " << j << "\n";
                sharedMem->buffers[j].isOccupied = false;
                break;
            }
        }

        ReleaseMutex(hMutex);
        Sleep(1500);
    }

    UnmapViewOfFile(sharedMem);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Specify 'writer' or 'reader' as argument.\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "writer") {
        WriterProcess();
    }
    else if (mode == "reader") {
        ReaderProcess();
    }
    else {
        std::cerr << "Invalid mode. Use 'writer' or 'reader'.\n";
    }

    return 0;
}
