/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

template <class T>
class SumState
{
public:
  SumState() : value(0) {}
  T    value;
  void update(const T *values, int size);
};

template <typename T>
struct MaxState {
    T value;
    bool initialized;
    
    MaxState() : initialized(false) {}
    
    void update(T* values, size_t count) {
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
    
    void update(T* values, size_t count) {
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
    
    void update(void* values, size_t count) {
        value += count;
    }
};

template <typename T>
struct AvgState {
    T sum;
    int64_t count;
    
    AvgState() : sum(0), count(0) {}
    
    void update(T* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            sum += values[i];
        }
        count += count;
    }
    
    float get_result() {
        return static_cast<float>(sum) / count;
    }
};
