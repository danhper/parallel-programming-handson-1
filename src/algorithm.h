#pragma once

#include <functional>
#include <memory>

#include <mtbb/task_group.h>

#include "helpers.h"

#define PARALLEL_MIN_ELEMS 1000

bool should_use_serial(long elements_count)
{
    return elements_count < PARALLEL_MIN_ELEMS;
}

bool should_use_serial(long left, long right)
{
    return should_use_serial(right - left + 1);
}

template<typename T, typename U>
long serial_create_mask(T* values, std::unique_ptr<int[]>& mask, long left, long right, U pivot,
                        std::function<int(T, U)> test)
{
    long count = 0;
    for (long i = left; i <= right; i++) {
        if (test(values[i], pivot)) {
            mask[i] = 1;
            count++;
        }
    }
    return count;
}

template<typename T, typename U>
long parallel_create_mask(T* values, std::unique_ptr<int[]>& mask, long left, long right, U pivot,
                          std::function<int(T, U)> test)
{
    if (should_use_serial(left, right)) {
        return serial_create_mask(values, mask, left, right, pivot, test);
    } else {
        long middle = (right + left) / 2;
        long left_count, right_count;
        mtbb::task_group tg;
        tg.run([&] {
            left_count = parallel_create_mask(values, mask, left, middle, pivot, test);
        });
        right_count = parallel_create_mask(values, mask, middle + 1, right, pivot, test);
        tg.wait();
        return left_count + right_count;
    }
}

template <typename T>
void serial_prefix_sum_prepass(std::unique_ptr<T[]>& values,
                               long left,
                               long right,
                               std::unique_ptr<T[]>& sums_tree,
                               std::unique_ptr<long[]>& leaf_counts_tree,
                               long tree_index)
{
    sums_tree[tree_index] = 0;
    for (long i = left; i <= right; i++) {
        sums_tree[tree_index] += values[i];
    }
    leaf_counts_tree[tree_index] = right - left + 1;
}

template <typename T>
void parallel_prefix_sum_prepass(std::unique_ptr<T[]>& values, long left, long right, std::unique_ptr<T[]>& sums_tree,
                                 std::unique_ptr<long[]>& leaf_counts_tree, long tree_index)
{
    if (should_use_serial(left, right)) {
        serial_prefix_sum_prepass(values, left, right, sums_tree, leaf_counts_tree, tree_index);
    } else {
        long middle = (right + left) / 2;
        long tree_left_index = tree_index * 2;
        long tree_right_index = tree_left_index + 1;
        mtbb::task_group tg;
        tg.run([&] {
            parallel_prefix_sum_prepass(values, left, middle,
                                        sums_tree, leaf_counts_tree, tree_left_index);
        });
        parallel_prefix_sum_prepass(values, middle + 1, right,
                                    sums_tree, leaf_counts_tree, tree_right_index);
        tg.wait();
        sums_tree[tree_index] = sums_tree[tree_left_index] + sums_tree[tree_right_index];
        leaf_counts_tree[tree_index] = leaf_counts_tree[tree_left_index] + leaf_counts_tree[tree_right_index];
    }
}

template <typename T, typename U>
void serial_prefix_sum_postpass(std::unique_ptr<T[]>& output,
                                std::unique_ptr<U[]>& values,
                                long index,
                                U sum,
                                std::unique_ptr<U[]>& sums_tree,
                                std::unique_ptr<long[]>& leaf_counts_tree,
                                long tree_index)
{
    for (long i = index; i < index + leaf_counts_tree[tree_index]; i++) {
        output[i] = sum + values[i];
        sum = output[i];
    }
}

template <typename T, typename U>
void parallel_prefix_sum_postpass(std::unique_ptr<T[]>& output,
                                  std::unique_ptr<U[]>& values,
                                  long index,
                                  U sum,
                                  std::unique_ptr<U[]>& sums_tree,
                                  std::unique_ptr<long[]>& leaf_counts_tree,
                                  long tree_index)
{
    if (should_use_serial(leaf_counts_tree[tree_index])) {
        serial_prefix_sum_postpass(output, values, index, sum, sums_tree, leaf_counts_tree, tree_index);
    } else {
        long tree_left_index = tree_index * 2;
        long tree_right_index = tree_left_index + 1;
        mtbb::task_group tg;
        tg.run([&] {
            parallel_prefix_sum_postpass(output, values, index, sum, sums_tree, leaf_counts_tree, tree_left_index);
        });
        parallel_prefix_sum_postpass(output, values, index + leaf_counts_tree[tree_left_index],
                            sum + sums_tree[tree_left_index], sums_tree, leaf_counts_tree, tree_right_index);
        tg.wait();
    }
}

// http://graphics.stanford.edu/~seander/bithacks.html
unsigned long upper_power_of_two(unsigned long v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

template <typename T, typename U>
void prefix_sum(std::unique_ptr<T[]>& values, std::unique_ptr<U[]>& output, long left, long right)
{
    auto n = upper_power_of_two(right - left + 1);
    auto sums_tree = std::make_unique<T[]>(2 * n);
    auto leaf_counts_tree = std::make_unique<long[]>(2 * n);
    parallel_prefix_sum_prepass(values, left, right, sums_tree, leaf_counts_tree, 1);
    parallel_prefix_sum_postpass(output, values, 0, 0, sums_tree, leaf_counts_tree, 1);
}

template <typename T>
void serial_copy(std::unique_ptr<T[]>& source,
                   T *destination,
                   long left,
                   long right,
                   long source_offset,
                   long destination_offset)
{
    for (long i = left; i <= right; i++) {
        destination[i + destination_offset] = source[i + source_offset];
    }
}

template <typename T>
void parallel_copy(std::unique_ptr<T[]>& source,
                   T *destination,
                   long left,
                   long right,
                   long source_offset,
                   long destination_offset)
{
    if (should_use_serial(left, right)) {
        serial_copy(source, destination, left, right, source_offset, destination_offset);
    } else {
        long middle = (right + left) / 2;
        mtbb::task_group tg;
        tg.run([&] {
            parallel_copy(source, destination, left, middle, source_offset, destination_offset);
        });
        parallel_copy(source, destination, middle + 1, right, source_offset, destination_offset);
        tg.wait();
    }
}

template <typename T>
void serial_copy_at_indexes(T* source,
                            std::unique_ptr<T[]>& destination,
                            std::unique_ptr<int[]>& mask,
                            std::unique_ptr<long[]>& indexes,
                            long left,
                            long right,
                            long source_offset,
                            long destination_offset)
{
    for (long i = left; i <= right; i++) {
        if (mask[i]) {
            destination[destination_offset + indexes[i] - 1] = source[i + source_offset];
        }
    }
}


template <typename T>
void parallel_copy_at_indexes(T* source,
                              std::unique_ptr<T[]>& destination,
                              std::unique_ptr<int[]>& mask,
                              std::unique_ptr<long[]>& indexes,
                              long left,
                              long right,
                              long source_offset,
                              long destination_offset)
{
    if (should_use_serial(left, right)) {
        serial_copy_at_indexes(source, destination, mask, indexes, left, right, source_offset, destination_offset);
    } else {
        long middle = (right + left) / 2;
        mtbb::task_group tg;
        tg.run([&]{
            parallel_copy_at_indexes(source, destination, mask, indexes, left, middle, source_offset, destination_offset);
        });
        parallel_copy_at_indexes(source, destination, mask, indexes, middle + 1, right, source_offset, destination_offset);
        tg.wait();
    }
}

template <typename T, typename U>
long parallel_partition(T* values, U pivot, long left, long right, std::function<int(T, U)> compare)
{
    long n = right - left + 1;

    std::unique_ptr<int[]> left_mask;
    std::unique_ptr<int[]> pivot_mask;
    std::unique_ptr<int[]> right_mask;

    long left_count;
    long pivot_count;

    mtbb::task_group mask_tg;
    mask_tg.run([&] {
        left_mask = std::make_unique<int[]>(n);
        left_count = parallel_create_mask<T, U>(values, left_mask, 0, n - 1, pivot,
                                                [&compare](T a, U b) { return compare(a, b) < 0; });
    });
    mask_tg.run([&] {
        pivot_mask = std::make_unique<int[]>(n);
        pivot_count = parallel_create_mask<T, U>(values, pivot_mask, 0, n - 1, pivot,
                                                 [&compare](T a, U b) { return compare(a, b) == 0; });
    });
    right_mask = std::make_unique<int[]>(n);
    parallel_create_mask<T, U>(values, right_mask, 0, n - 1, pivot,
                               [&compare](T a, U b) { return compare(a, b) > 0; });
    mask_tg.wait();

    std::unique_ptr<long[]> left_indexes;
    std::unique_ptr<long[]> pivot_indexes;
    std::unique_ptr<long[]> right_indexes;

    mtbb::task_group sum_tg;
    sum_tg.run([&] {
        left_indexes = std::make_unique<long[]>(n);
        prefix_sum(left_mask, left_indexes, 0, n - 1);
    });
    sum_tg.run([&] {
        pivot_indexes = std::make_unique<long[]>(n);
        prefix_sum(pivot_mask, pivot_indexes, 0, n - 1);
    });
    right_indexes = std::make_unique<long[]>(n);
    prefix_sum(right_mask, right_indexes, 0, n - 1);
    sum_tg.wait();

    auto output = std::make_unique<T[]>(n);

    mtbb::task_group copy_tg;
    copy_tg.run([&]{
        parallel_copy_at_indexes(values, output, left_mask, left_indexes, 0, n - 1, left, 0);
    });
    copy_tg.run([&]{
        parallel_copy_at_indexes(values, output, pivot_mask, pivot_indexes, 0, n - 1, left, left_count);
    });
    parallel_copy_at_indexes(values, output, right_mask, right_indexes, 0, n - 1, left, left_count + pivot_count);
    copy_tg.wait();

    parallel_copy(output, values, 0, n - 1, 0, left);

    return left_count;
}
