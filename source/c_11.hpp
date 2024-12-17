// c11标准 特性举例
#pragma once
#include <iostream>

void c11_example() {
    // 类型推导
    auto x = 10;
    std::cout <<"type of x is "<< typeid(x).name() << std::endl;

    // 匿名函数
    auto sum = [](int a, int b) {
        return a + b;
    };
    std::cout << sum(1, 2) << std::endl;

    // 字面量
    auto str = R"(This is a s\tring)";
    std::cout << str << std::endl;

    // 结构化绑定
    struct Point {
        int x, y;
    };
    auto p = Point{1, 2};
    std::cout << p.x << " " << p.y << std::endl;

    // 统一初始化
    int a = {10};
    std::cout << a << std::endl;

    // 右值引用
    int&& r = 10;
    std::cout << r << std::endl;

    // 移动语义
    std::string s1 = "hello";
    std::string s2 = std::move(s1);
    std::cout << s2 << std::endl;

}