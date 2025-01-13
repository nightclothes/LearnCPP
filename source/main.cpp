#include "test_c11.hpp"
#include "test_spdlog.hpp"
#include "test_imgui.hpp"
#include <iostream>
#include <thread>
#include <chrono>
int main() {
    std::cout << "Hello, world!1111" << std::endl;
    imgui_example();
    //暂停5s  
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // FIXME 你好
    // NOTE 你好
    // BUG 你好
    // TODO 你好
    return 0;
}