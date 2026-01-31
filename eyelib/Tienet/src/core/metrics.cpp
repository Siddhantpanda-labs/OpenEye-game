/**
 * @file metrics.cpp
 */

#include "tienet/core/metrics.hpp"
#include <sstream>

namespace tienet {
namespace core {

Counter& MetricsRegistry::counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_counters[name];
}

Gauge& MetricsRegistry::gauge(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gauges[name];
}

Histogram& MetricsRegistry::histogram(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_histograms[name];
}

std::string MetricsRegistry::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\"counters\":{";
    bool first = true;
    for (const auto& kv : m_counters) {
        if (!first) oss << ","; first = false;
        oss << "\"" << kv.first << "\":" << kv.second.get();
    }
    oss << "},\"gauges\":{";
    first = true;
    for (const auto& kv : m_gauges) {
        if (!first) oss << ","; first = false;
        oss << "\"" << kv.first << "\":" << kv.second.get();
    }
    oss << "},\"histograms\":{";
    first = true;
    for (const auto& kv : m_histograms) {
        if (!first) oss << ","; first = false;
        oss << "\"" << kv.first << "\":[";
        for (size_t i = 0; i < kv.second.values().size(); ++i) {
            if (i) oss << ",";
            oss << kv.second.values()[i];
        }
        oss << "]";
    }
    oss << "}}";
    return oss.str();
}

} // namespace core
} // namespace tienet


