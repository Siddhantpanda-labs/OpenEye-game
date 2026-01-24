#pragma once

#include "core/job/ThreadPool.h"
#include <cstddef>
#include <functional>
#include <iterator>
#include <algorithm>
#include <thread>

namespace eyelib {
namespace core {
namespace job {

/** Parallel loop over [begin, end). Chunked for cache efficiency. */
inline void ParallelFor(size_t begin, size_t end, std::function<void(size_t)> fn) {
    if (begin >= end || !fn) return;
    const size_t n = end - begin;
    const size_t nWorkers = (std::max)(size_t(1), size_t(std::thread::hardware_concurrency()));
    const size_t chunkSize = (std::max)(size_t(1), n / (nWorkers * 2));
    std::vector<std::function<void()>> tasks;
    for (size_t i = begin; i < end; i += chunkSize) {
        const size_t lo = i;
        const size_t hi = (std::min)(i + chunkSize, end);
        tasks.emplace_back([fn, lo, hi] {
            for (size_t k = lo; k < hi; ++k) fn(k);
        });
    }
    ThreadPool pool(0);
    pool.SubmitBatch(tasks);
    pool.Wait();
}

/** Parallel foreach over container. Chunked by index. */
template <typename Container, typename Fn>
void ParallelForEach(Container& container, Fn fn) {
    using std::begin;
    using std::end;
    auto b = begin(container);
    auto e = end(container);
    const size_t n = static_cast<size_t>(std::distance(b, e));
    if (n == 0) return;
    ParallelFor(size_t(0), n, [&](size_t i) {
        auto it = b;
        std::advance(it, static_cast<std::ptrdiff_t>(i));
        fn(*it);
    });
}

} // namespace job
} // namespace core
} // namespace eyelib
