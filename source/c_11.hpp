// c11标准 特性举例
#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include <assert.h>
#include <thread>
#include <atomic>

template<typename T>
T sum(T a) {
    return a;
}
template<typename T, typename... Args>
T sum(T first, Args... args) {
    return first + sum(args...);
}

void c11_example() {
    
    
    std::cout << "c11 feature" << std::endl;
    
    // 类型推导
    auto x = 5; // int
    auto y = 3.14; // double
    auto z = "Hello"; // const char[]

    // range-based for loop
    std::vector<int> v = {1, 2, 3, 4, 5};
    for (int i : v) {
        std::cout << i << ' ';
    }

    // lambda expression
    auto lambda = [](int x) { return x * x; };
    int yy = lambda(5); // y = 25

    // 智能指针
    std::unique_ptr<int> up(new int(10));
    std::shared_ptr<int> sp(new int(20));

    // nullptr
    int* p = nullptr;

    // 静态断言
    static_assert(sizeof(int) == 4, "int is not 4 bytes");

    // 线程局部存储
    thread_local int tls = 0;
    tls = 10;

    // 原子操作
    std::atomic<int> ai(10);
    ai.fetch_add(5); // 15
    ai.fetch_sub(3); // 12
    std::cout << ai << std::endl;

    // 变长参数模版
    std::cout << "Sum: " << sum(1, 2, 3, 4, 5) << std::endl;

}