#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <iostream>
#include <unordered_map>
#include <string>

struct Timer {
    std::unordered_map<std::string, double> times;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> start;

    void begin(const std::string& name) {
        start[name] = std::chrono::high_resolution_clock::now();
    }

    void end(const std::string& name) {
        auto stop = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(stop - start[name]).count();
        times[name] += ms;
    }

    void print() {
        std::cout << "\n===== PROFILING =====\n";
        for (auto& p : times) {
            std::cout << p.first << ": " << p.second << " ms\n";
        }
    }
};


#endif