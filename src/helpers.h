#pragma once

#include <functional>
#include <iostream>
#include <chrono>
#include "assert.h"

template<typename T>
void output_array(T* array, long length)
{
    for (long i = 0; i < length; i++) {
        std::cout << array[i] << " ";
    }
    std::cout << std::endl;
}

void print_time_diff(std::string name,
                     std::chrono::high_resolution_clock::time_point start,
                     std::chrono::high_resolution_clock::time_point end)
{
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "executed " << name << " in " << duration << "ms" << std::endl;
}

void with_exec_time(std::string name, std::function<void()> f)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    f();
    auto t2 = std::chrono::high_resolution_clock::now();
    print_time_diff(name, t1, t2);
}

template <typename T>
void check_partition(T* array, long n, T pivot)
{
    int state = 0;
    for (long i = 0; i < n; i++) {
        switch (state) {
            case 0:
            assert(array[i] <= pivot);
            if (array[i] == pivot) {
                state = 1;
            }
            break;
            case 1:
            assert(array[i] >= pivot);
            if (array[i] > pivot) {
                state = 1;
            }
            break;
            case 2:
            assert(array[i] > pivot);
        }
    }
}
