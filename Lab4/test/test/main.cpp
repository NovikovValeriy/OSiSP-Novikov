#include <windows.h>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

int NUM_PHILOSOPHERS = 5; 
int THINK_TIME_MIN = 1000;
int THINK_TIME_MAX = 3000;
int EAT_TIME_MIN = 1000;  
int EAT_TIME_MAX = 2000;  
bool USE_PRIORITY = false;
int TIME_OUT = 10;

HANDLE* forks;
HANDLE* philosophers;
std::mutex print_mutex;

std::atomic<int>* successful_eats;
std::atomic<int>* blocked_attempts;
std::atomic<int> total_eats(0);
std::atomic<int> total_blocks(0);

std::mt19937 rng(std::random_device{}());
std::uniform_int_distribution<int> think_dist(THINK_TIME_MIN, THINK_TIME_MAX);
std::uniform_int_distribution<int> eat_dist(EAT_TIME_MIN, EAT_TIME_MAX);

DWORD WINAPI philosopher(LPVOID param) {
    int id = (int)param;
    int leftFork = id;
    int rightFork = (id + 1) % NUM_PHILOSOPHERS;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "Philosopher " << id << " is thinking.\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(think_dist(rng)));

        bool acquired = false;

        if (USE_PRIORITY) {
            HANDLE index1, index2;
            if (id % 2 == 0) {
                index1 = forks[leftFork];
                index2 = forks[rightFork];
            }
            else {
                index1 = forks[rightFork];
                index2 = forks[leftFork];
            }
            WaitForSingleObject(index1, INFINITE);
            if (WaitForSingleObject(index2, TIME_OUT < 0 ? INFINITE : TIME_OUT) == WAIT_OBJECT_0) { //10
                acquired = true;
            }
            else {
                ReleaseMutex(index1);
            }
        }
        else {
            WaitForSingleObject(forks[leftFork], INFINITE);
            WaitForSingleObject(forks[rightFork], INFINITE);
            acquired = true;
        }

        if (acquired) {
            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "Philosopher " << id << " is eating.\n";
            }
            successful_eats[id]++;
            total_eats++;
            std::this_thread::sleep_for(std::chrono::milliseconds(eat_dist(rng)));

            ReleaseMutex(forks[leftFork]);
            ReleaseMutex(forks[rightFork]);
        }
        else {
            blocked_attempts[id]++;
            total_blocks++;
            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "Philosopher " << id << " is blocked.\n";
            }
        }
    }

    return 0;
}

void print_statistics() {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n=== Simulation Results ===\n";
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        std::cout << "Philosopher " << i << ":\n";
        std::cout << "  Successful eats: " << successful_eats[i] << "\n";
        std::cout << "  Blocked attempts: " << blocked_attempts[i] << "\n";
    }
    std::cout << "Total successful eats: " << total_eats << "\n";
    std::cout << "Total blocked attempts: " << total_blocks << "\n";
    double efficiency = (double)total_eats / (total_eats + total_blocks) * 100.0;
    std::cout << "Overall efficiency: " << efficiency << "%\n";
}

int main() {
    std::cout << "Enter number of philosophers: ";
    std::cin >> NUM_PHILOSOPHERS;

    std::cout << "Use priority strategy? (1 - yes, 0 - no): ";
    std::cin >> USE_PRIORITY;
    if (USE_PRIORITY) {
        std::cout << "Enter timeout(milliseconds), enter -1 for INFINITE timeout: ";
        std::cin >> TIME_OUT;
    }

    forks = new HANDLE[NUM_PHILOSOPHERS];
    philosophers = new HANDLE[NUM_PHILOSOPHERS];
    successful_eats = new std::atomic<int>[NUM_PHILOSOPHERS]();
    blocked_attempts = new std::atomic<int>[NUM_PHILOSOPHERS]();


    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        forks[i] = CreateMutex(NULL, FALSE, NULL);
        if (forks[i] == NULL) {
            std::cerr << "Failed to create mutex for fork " << i << ".\n";
            return 1;
        }
        successful_eats[i] = 0;
        blocked_attempts[i] = 0;
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        philosophers[i] = CreateThread(NULL, 0, philosopher, (LPVOID)i, 0, NULL);
        if (philosophers[i] == NULL) {
            std::cerr << "Failed to create thread for philosopher " << i << ".\n";
            return 1;
        }
    }

    //std::this_thread::sleep_for(std::chrono::seconds(30));
    Sleep(30000);

    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        TerminateThread(philosophers[i], 0);
        CloseHandle(philosophers[i]);
        CloseHandle(forks[i]);
    }

    print_statistics();

    delete[] forks;
    delete[] philosophers;
    delete[] successful_eats;
    delete[] blocked_attempts;

    return 0;
}
