#include "test_c11.hpp"
#include "test_spdlog.hpp"
#include <iostream>
#include <thread>
#include <chrono>
int main() {
    std::cout << "Hello, world!" << std::endl;
    spdlog_example();
    //暂停5s
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}