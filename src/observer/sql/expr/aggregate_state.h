#include <type_traits>

#include "sql/expr/aggregate_state.h"

#ifdef USE_SIMD
#include "common/math/simd_util.h"
#endif
template <typename T>
void SumState<T>::update(const T *values, int size)
{
#ifdef USE_SIMD
  if constexpr (std::is_same<T, float>::value) {
    value += mm256_sum_ps(values, size);
  } else if constexpr (std::is_same<T, int>::value) {
    value += mm256_sum_epi32(values, size);
  }
#else
  for (int i = 0; i < size; ++i) {
 	  value += values[i];
  }
#endif
}

template class SumState<int>;
template class SumState<float>;
