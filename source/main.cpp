#include "c_11.hpp"
#include <iostream>
#include <thread>
#include <chrono>
int main() {
    std::cout << "Hello, world!" << std::endl;
    c11_example();
    //暂停5s
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}