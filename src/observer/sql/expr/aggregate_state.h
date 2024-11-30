#pragma once

#include <cstddef>  // for size_t
#include <algorithm>  // for std::max/min
#include "storage/field/field.h"

template <typename T>
struct MaxState {
    T value;
    bool initialized;
    
    MaxState() : initialized(false) {}
    
    void update(const T* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (!initialized) {
                value = values[i];
                initialized = true;
            } else {
                value = std::max(value, values[i]);
            }
        }
    }
};

template <typename T>
struct MinState {
    T value;
    bool initialized;
    
    MinState() : initialized(false) {}
    
    void update(const T* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (!initialized) {
                value = values[i];
                initialized = true;
            } else {
                value = std::min(value, values[i]);
            }
        }
    }
};

struct CountState {
    int64_t value;
    
    CountState() : value(0) {}
    
    void update(const void* /*values*/, size_t count) {
        value += static_cast<int64_t>(count);
    }
};

template <typename T>
struct AvgState {
    T sum;
    int64_t count;
    
    AvgState() : sum(0), count(0) {}
    
    void update(const T* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            sum += values[i];
        }
        count += static_cast<int64_t>(count);
    }
    
    float get_result() const {
        return count > 0 ? static_cast<float>(sum) / count : 0.0f;
    }
};
