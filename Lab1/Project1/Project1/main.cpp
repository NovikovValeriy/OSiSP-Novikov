#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>

class ThreadData {
public:
    int threadId;
    int size;
    long long startRow;
    long long rowCount;
    double percent;
    COORD cursorPos;
    std::vector<std::vector<int>>& A;
    std::vector<std::vector<int>>& B;
    std::vector<std::vector<int>>& C;

    ThreadData(int id, long long start, long long count, COORD pos,
        std::vector<std::vector<int>>& a,
        std::vector<std::vector<int>>& b,
        std::vector<std::vector<int>>& c,
        int msize)
        : threadId(id), startRow(start), rowCount(count), cursorPos(pos), A(a), B(b), C(c), percent(0), size(msize) {}
};

CRITICAL_SECTION consoleLock;

void MoveCursorToPosition(int x, int y) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD position;
    position.X = x;
    position.Y = y;
    SetConsoleCursorPosition(hConsole, position);
}

void HideCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;

    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

void ShowCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;

    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

DWORD WINAPI ProcessMatrixSegment(LPVOID lpParam) {
    ThreadData* data = static_cast<ThreadData*>(lpParam);

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    long long elementsCount = data->rowCount * data->size;
    long long counter = 0;
    for (long long i = data->startRow; i < data->rowCount + data->startRow; ++i) {
        data->percent = (i + 1) * 100.0 / data->rowCount;
        for (long long j = 0; j < data->size; ++j) {
            data->C[i][j] = 0;
            for (long long k = 0; k < data->size; ++k) {
                data->C[i][j] += data->A[i][k] * data->B[k][j];
            }
            ++counter;
            double up = ((i - data->startRow) * data->size + j);
            double down = data->rowCount * data->size;
            data->percent = ((double)counter / (double)elementsCount) * 100.0;
            EnterCriticalSection(&consoleLock);
            MoveCursorToPosition(data->cursorPos.X, data->cursorPos.Y);
            std::cout << std::fixed << std::setprecision(2)
                << "Поток #" << data->threadId << "\t-\t " << data->percent << "%   ";
            LeaveCriticalSection(&consoleLock);
            //Sleep(100);
        }
    }
    QueryPerformanceCounter(&end);
    double elapsedTime = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    EnterCriticalSection(&consoleLock);
    MoveCursorToPosition(data->cursorPos.X, data->cursorPos.Y);
    std::cout << "\t\t\t";
    MoveCursorToPosition(data->cursorPos.X, data->cursorPos.Y);
    std::cout << std::fixed << std::setprecision(2)
        << "Поток #" << data->threadId << "\tЗавершен за " << elapsedTime << " секунд   ";
    LeaveCriticalSection(&consoleLock);
    return 0;
}

double GetCPUUsage(FILETIME* preIdleTime, FILETIME* preKernelTime, FILETIME* preUserTime) {
    FILETIME idleTime, kernelTime, userTime;

    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULONGLONG idle = (*(ULONGLONG*)&idleTime) - (*(ULONGLONG*)preIdleTime);
        ULONGLONG kernel = (*(ULONGLONG*)&kernelTime) - (*(ULONGLONG*)preKernelTime);
        ULONGLONG user = (*(ULONGLONG*)&userTime) - (*(ULONGLONG*)preUserTime);

        ULONGLONG total = kernel + user;

        if (total > 0) {
            return (double)(total - idle) * 100.0 / total;
        }
    }
    return 0.0;
}

int main() {
    setlocale(0, "");
    InitializeCriticalSection(&consoleLock);

    int threadCount;
    int MATRIX_SIZE;
    std::cout << "Введите размер матриц: ";
    std::cin >> MATRIX_SIZE;
    if (MATRIX_SIZE <= 1) return 1;
    std::vector<std::vector<int>> A(MATRIX_SIZE, std::vector<int>(MATRIX_SIZE, 1)); // Матрица A
    std::vector<std::vector<int>> B(MATRIX_SIZE, std::vector<int>(MATRIX_SIZE, 2)); // Матрица B
    std::vector<std::vector<int>> C(MATRIX_SIZE, std::vector<int>(MATRIX_SIZE, 0)); // Матрица результата C

    std::cout << "Введите количество потоков: ";
    std::cin >> threadCount;
    if (threadCount <= 0 || threadCount > MATRIX_SIZE) return 1;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD originalPosition = csbi.dwCursorPosition;
    COORD pos = { 0, 0 };
    long long rowCount = MATRIX_SIZE / threadCount;
    long long lastRowCount = rowCount + MATRIX_SIZE % threadCount;

    HANDLE* threads = new HANDLE[threadCount];
    std::vector<ThreadData*> threadData;
    int i = 0;
    for (i = 0; i < threadCount - 1; i++)
    {
        pos.X = originalPosition.X;
        pos.Y = originalPosition.Y + i;
        threadData.push_back(new ThreadData(i + 1, rowCount * i, rowCount, pos, A, B, C, MATRIX_SIZE));
    }
    pos.X = originalPosition.X;
    pos.Y = originalPosition.Y + i;
    threadData.push_back(new ThreadData(i + 1, rowCount * i, lastRowCount, pos, A, B, C, MATRIX_SIZE));

    HideCursor();
    FILETIME preIdleTime, preKernelTime, preUserTime;
    GetSystemTimes(&preIdleTime, &preKernelTime, &preUserTime);
    for (int i = 0; i < threadCount; ++i) {
        threads[i] = CreateThread(NULL, 0, ProcessMatrixSegment, threadData[i], 0, NULL);

        if (threads[i] == NULL) {
            std::cerr << "Ошибка при создании потока " << i + 1 << std::endl;
            return 1;
        }
    }
    WaitForMultipleObjects(threadCount, threads, TRUE, INFINITE);
    for (int i = 0; i < threadCount; ++i) {
        CloseHandle(threads[i]);
    }
    MoveCursorToPosition(0, threadCount + 2);
    double cpuUsage = GetCPUUsage(&preIdleTime, &preKernelTime, &preUserTime);
    std::cout << "Использование ЦП: " << cpuUsage << "%" << std::endl;
    std::cout << "Результат:\n";
    for (long long i = 0; i < MATRIX_SIZE; ++i) {
        for (long long j = 0; j < MATRIX_SIZE; ++j) {
            std::cout << C[i][j] << " ";
        }
        std::cout << std::endl;
    }

    delete[] threads;
    DeleteCriticalSection(&consoleLock);
    ShowCursor();
    return 0;
}