// SPDX-FileCopyrightText: 2017 - 2024 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#include "core/matrix/batch_csr_kernels.hpp"

#include <thrust/functional.h>

#include <ginkgo/core/base/batch_multi_vector.hpp>
#include <ginkgo/core/base/types.hpp>
#include <ginkgo/core/matrix/batch_csr.hpp>

#include "common/cuda_hip/base/config.hpp"
#include "common/cuda_hip/base/runtime.hpp"
#include "common/cuda_hip/base/thrust.hpp"
#include "common/cuda_hip/components/cooperative_groups.hpp"
#include "common/cuda_hip/components/reduction.hpp"
#include "common/cuda_hip/components/thread_ids.hpp"
#include "common/cuda_hip/components/uninitialized_array.hpp"
#include "core/base/batch_struct.hpp"
#include "core/matrix/batch_struct.hpp"
#include "hip/base/batch_struct.hip.hpp"
#include "hip/matrix/batch_struct.hip.hpp"


namespace gko {
namespace kernels {
namespace hip {
/**
 * @brief The Csr matrix format namespace.
 * @ref Csr
 * @ingroup batch_csr
 */
namespace batch_csr {


constexpr auto default_block_size = 256;
constexpr int sm_oversubscription = 4;

// clang-format off

// NOTE: DO NOT CHANGE THE ORDERING OF THE INCLUDES

#include "common/cuda_hip/matrix/batch_csr_kernels.hpp.inc"


#include "common/cuda_hip/matrix/batch_csr_kernel_launcher.hpp.inc"

// clang-format on


}  // namespace batch_csr
}  // namespace hip
}  // namespace kernels
}  // namespace gko
