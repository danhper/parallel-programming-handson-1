#include <iostream>
#include <cstdlib>
#include <ctime>

#include "algorithm.h"

template <typename T>
T basic_partition(T* array, long n)
{
    long pivot_index = rand() % n;
    T pivot = array[pivot_index];
    array[pivot_index] = array[0];
    array[0] = pivot;
    long i, h;
    for (i = 1, h = 1; i < n; i++) {
        if (array[i] < pivot) {
            T tmp = array[h];
            array[h] = array[i];
            array[i] = tmp;
            h++;
        }
    }
    array[0] = array[h - 1];
    array[h - 1] = pivot;
    return pivot;
}

int main(void)
{
    std::srand(std::time(0));

    long n = 10000000;
    int* array = new int[n];
    int max_val = 10000;
    for (long i = 0; i < n; i++) {
        array[i] = std::rand() % max_val;
    }

    int pivot = max_val / 2;
    with_exec_time("parallel_partition", [&]() {
        parallel_partition<int, int>(array, pivot, 0, n - 1, [](int a, int b) { return a - b; });
    });

    // int pivot;
    // with_exec_time("basic_partition", [&]() {
    //     pivot = basic_partition<int>(array, n);
    // });

    check_partition(array, n, pivot);

    delete[] array;
}
