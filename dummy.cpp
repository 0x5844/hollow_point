#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>

int main() {
    std::cout << "Dummy process running with PID: " << getpid() << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}