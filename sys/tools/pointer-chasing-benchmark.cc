#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>

#include "absl/random/random.h"
#include "benchmark/benchmark.h"

namespace sys {
namespace {

void BM_pointer_chasing(benchmark::State &state) {
    int64_t num_elements = state.range(0) / sizeof(uintptr_t);
    auto array = std::make_unique<uintptr_t[]>(num_elements);
    std::iota(&array[0], &array[num_elements], 0);
    std::shuffle(&array[0], &array[num_elements], absl::InsecureBitGen());
    uintptr_t index = 0;
    for (auto _ : state) {
        index = array[index];
    }
    benchmark::DoNotOptimize(index);
}

BENCHMARK(BM_pointer_chasing)->RangeMultiplier(2)->Range(4096, 1073741824);

}  // namespace
}  // namespace sys
