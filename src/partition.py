def swap(array, i, j):
    array[i], array[j] = array[j], array[i]

def create_mask(numbers, mask, left, right, pivot, test):
    if left == right:
        mask[left] = 1 if test(numbers[left], pivot) else 0
    else:
        middle = (right + left) // 2
        create_mask(numbers, mask, left, middle, pivot, test)
        create_mask(numbers, mask, middle + 1, right, pivot, test)

def create_array(numbers, mask, indexes, output, left, right):
    if left == right:
        if mask[left]:
            output[indexes[left] + 1] = numbers[left]
    else:
        middle = (right + left) // 2
        create_array(numbers, mask, indexes, output, left, middle)
        create_array(numbers, mask, indexes, output, middle + 1, right)

def create_indexes(mask):
    indexes = []
    current_sum = 0
    for n in mask:
        indexes.append(current_sum)
        current_sum += n
    return indexes


def do_partition(numbers, pivot):
    # mask = [1 if n < pivot else 0 for n in numbers]
    lower_mask = [0 for _ in numbers]
    create_mask(numbers, lower_mask, 0, len(numbers) - 1, pivot, lambda a, b: a < b)
    lower_indexes = create_indexes(lower_mask)


    output = [0 for _ in numbers]
    create_array(numbers, mask, indexes, output, 0, len(numbers) - 1)
    print(numbers)
    print(mask)
    print(indexes)
    print(output)
    return indexes[-1]


def partition(numbers, pivot_index):
    pivot = numbers[pivot_index]
    numbers[0], numbers[pivot_index] = pivot, numbers[0]
    n = do_partition(numbers, pivot)
    numbers[n], numbers[0] = numbers[0], numbers[n]
    return n

def main():
    # numbers = [1, 8, 4, 5, 9, 3, 2, 10, 0, 11, 12, 13, 2, 3, -1, 43, 1]
    numbers = [6, 1, 7, 4, 0, 3, 5, 2]
    n = partition(numbers, 3)

if __name__ == '__main__':
    main()
