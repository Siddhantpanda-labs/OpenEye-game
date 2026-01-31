/**
 * @file metrics.hpp
 * @brief Minimal metrics registry (counters, gauges, histograms)
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace tienet {
namespace core {

class Counter {
public:
    void add(uint64_t v) { m_value += v; }
    uint64_t get() const { return m_value; }
private:
    uint64_t m_value = 0;
};

class Gauge {
public:
    void set(int64_t v) { m_value = v; }
    int64_t get() const { return m_value; }
private:
    int64_t m_value = 0;
};

class Histogram {
public:
    void observe(double v) { m_values.push_back(v); }
    const std::vector<double>& values() const { return m_values; }
private:
    std::vector<double> m_values;
};

class MetricsRegistry {
public:
    Counter& counter(const std::string& name);
    Gauge& gauge(const std::string& name);
    Histogram& histogram(const std::string& name);
    std::string toJson() const;
private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Counter> m_counters;
    std::unordered_map<std::string, Gauge> m_gauges;
    std::unordered_map<std::string, Histogram> m_histograms;
};

} // namespace core
} // namespace tienet


