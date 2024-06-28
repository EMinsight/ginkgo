// SPDX-FileCopyrightText: 2017 - 2024 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GKO_HIP_COMPONENTS_ATOMIC_HIP_HPP_
#define GKO_HIP_COMPONENTS_ATOMIC_HIP_HPP_


#include <type_traits>

#include "common/cuda_hip/base/types.hpp"
#include "hip/base/math.hip.hpp"


namespace gko {
namespace kernels {
namespace hip {


#include "common/cuda_hip/components/atomic.hpp.inc"


}  // namespace hip
}  // namespace kernels
}  // namespace gko


#endif  // GKO_HIP_COMPONENTS_ATOMIC_HIP_HPP_
