#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>

const int BUFFER_SIZE = 1024;
const int NUM_ASYNC_OPERATIONS = 4;

struct AsyncContext {
    OVERLAPPED overlapped;
    std::vector<int> buffer;
    HANDLE hFile;
};

void GenerateTestFile(const wchar_t* filename, size_t dataSize) {
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error creating test file." << std::endl;
        return;
    }

    std::vector<int> data(dataSize / sizeof(int));
    for (auto& val : data) val = rand();
    DWORD written;
    WriteFile(hFile, data.data(), dataSize, &written, NULL);
    CloseHandle(hFile);
}

void CALLBACK ReadCompletionRoutine(DWORD errorCode, DWORD bytesTransferred, LPOVERLAPPED lpOverlapped) {
    if (errorCode != 0) {
        std::cerr << "Read failed with error code: " << errorCode << std::endl;
        return;
    }

    AsyncContext* context = reinterpret_cast<AsyncContext*>(lpOverlapped);
    size_t elementsRead = bytesTransferred / sizeof(int);

    std::sort(context->buffer.begin(), context->buffer.begin() + elementsRead);

    WriteFileEx(context->hFile, context->buffer.data(), bytesTransferred, &context->overlapped, [](DWORD errorCode, DWORD bytesWritten, LPOVERLAPPED lpOverlapped) {
        if (errorCode != 0) {
            std::cerr << "Write failed with error code: " << errorCode << std::endl;
        }
        });
}

void AsyncFileProcessing(const wchar_t* filename) {
    HANDLE hFile = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file for async reading and writing." << std::endl;
        return;
    }

    std::vector<AsyncContext> contexts(NUM_ASYNC_OPERATIONS);
    size_t offset = 0;

    for (int i = 0; i < NUM_ASYNC_OPERATIONS; ++i) {
        contexts[i].buffer.resize(BUFFER_SIZE / sizeof(int));
        contexts[i].hFile = hFile;
        contexts[i].overlapped = { 0 };
        contexts[i].overlapped.Offset = offset & 0xFFFFFFFF;
        contexts[i].overlapped.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;

        if (!ReadFileEx(hFile, contexts[i].buffer.data(), BUFFER_SIZE, &contexts[i].overlapped, ReadCompletionRoutine)) {
            std::cerr << "Error starting async read operation." << std::endl;
            CloseHandle(hFile);
            return;
        }
        offset += BUFFER_SIZE;
    }

    while (HasOverlappedIoCompleted(&contexts.back().overlapped) == 0) {
        SleepEx(INFINITE, TRUE);
    }

    CloseHandle(hFile);
}

void SyncFileProcessing(const wchar_t* filename) {
    HANDLE hFile = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file for sync reading and writing." << std::endl;
        return;
    }

    std::vector<int> buffer(BUFFER_SIZE / sizeof(int));
    DWORD bytesRead, bytesWritten;
    LARGE_INTEGER offset = { 0 };

    while (ReadFile(hFile, buffer.data(), BUFFER_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        std::sort(buffer.begin(), buffer.begin() + bytesRead / sizeof(int));

        SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN);

        if (!WriteFile(hFile, buffer.data(), bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            std::cerr << "Error writing to file." << std::endl;
            CloseHandle(hFile);
            return;
        }

        offset.QuadPart += bytesRead;
    }

    CloseHandle(hFile);
}

int main() {
    const wchar_t* filename = L"test_data.bin";
    size_t dataSize = 1 * BUFFER_SIZE;

    GenerateTestFile(filename, dataSize);

    auto startAsync = std::chrono::high_resolution_clock::now();
    AsyncFileProcessing(filename);
    auto endAsync = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> durationAsync = endAsync - startAsync;
    std::cout << "Asynchronous file processing and writing time: " << durationAsync.count() << " seconds" << std::endl;

    auto startSync = std::chrono::high_resolution_clock::now();
    SyncFileProcessing(filename);
    auto endSync = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> durationSync = endSync - startSync;
    std::cout << "Synchronous file processing and writing time: " << durationSync.count() << " seconds" << std::endl;

    return 0;
}
