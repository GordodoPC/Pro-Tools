#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <concepts>
#include <iostream>
#include <ranges>
#include <span>
#include <vector>

template<std::integral T>
constexpr T twice(T value) { return value * 2; }
static_assert(twice(21) == 42);

int main()
{
    std::vector<int> values{1, 2, 3, 4};
    std::span<const int> view{values};
    int sum = 0;
    for (int value : view | std::views::filter([](int n) { return n % 2 == 0; }))
        sum += value;
    if (sum != 6 || GetCurrentProcessId() == 0)
        return 1;
    std::cout << "C++20 and Windows SDK: PASS\n";
}
