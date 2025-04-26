/*
c++基本知识
*/
#include <iostream>
#include <string>
#include <math.h>
void base_cpp()
{

    // 1. 基本数据类型 - int
    int max_int = std::pow(2.0,31) - 1; // 整数类型的最大值2^31-1 int占用4字节
    int min_int = -1* (std::pow(2.0,31) - 1); // 整数类型的最小值-1*(2^31-1)
    std::cout << "int range: " << min_int << " to " << max_int << std::endl;
    
}