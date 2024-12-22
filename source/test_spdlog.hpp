// spdlog 功能测试
#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>
void spdlog_example(){
    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);
    
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    spdlog::info("Support for floats {:03.2f}", 1.23456);
    spdlog::info("Positional args are {1} {0}..", "too", "supported");
    spdlog::info("{:<30}", "left aligned");
    
    spdlog::set_level(spdlog::level::trace); // Set global log level to debug
    spdlog::trace("This message should be displayed..");   

    
    // change log pattern
    spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");
    
    // Compile time log levels
    // Note that this does not change the current log level, it will only
    // remove (depending on SPDLOG_ACTIVE_LEVEL) the call on the release code.
    SPDLOG_TRACE("Some trace message with param {}", 42);
    SPDLOG_DEBUG("Some debug message");

    // file logger
    try 
    {
        auto logger = spdlog::basic_logger_mt("basic_logger", "logs/basic-log.txt");
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }

    // Debug messages can be stored in a ring buffer instead of being logged immediately.
    // This is useful to display debug logs only when needed (e.g. when an error happens).
    // When needed, call dump_backtrace() to dump them to your log.
    spdlog::enable_backtrace(32); // Store the latest 32 messages in a buffer. 
    // or my_logger->enable_backtrace(32)..
    for(int i = 0; i < 100; i++)
    {
    spdlog::debug("Backtrace message {}", i); // not logged yet..
    }
    // e.g. if some error happened:
    spdlog::dump_backtrace(); // log them now! show the last 32 messages
    // or my_logger->dump_backtrace(32)..

}