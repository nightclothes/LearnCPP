// c11标准 特性举例
#pragma once
#include <iostream>

void c11_example() {
    auto x = 10;
    std::cout <<"type of x is "<< typeid(x).name() << std::endl;

    auto sum = [](int a, int b) {
        return a + b;
    };
    std::cout << sum(1, 2) << std::endl;

    auto str = R"(This is a s\tring)";
    std::cout << str << std::endl;

}