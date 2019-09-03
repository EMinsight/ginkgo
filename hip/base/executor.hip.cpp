/*******************************<GINKGO LICENSE>******************************
Copyright (c) 2017-2019, the Ginkgo authors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************<GINKGO LICENSE>*******************************/

#include <ginkgo/core/base/executor.hpp>


#include <iostream>


#include <hip/hip_runtime.h>


#include <ginkgo/config.hpp>
#include <ginkgo/core/base/exception_helpers.hpp>


#include "hip/base/device_guard.hip.hpp"
#include "hip/base/hipblas_bindings.hip.hpp"
#include "hip/base/hipsparse_bindings.hip.hpp"


namespace gko {
namespace {


// TODO: Fix this
inline int convert_sm_ver_to_cores(int major, int minor)
{
    // Defines for GPU Architecture types (using the SM version to determine
    // the # of cores per SM
    typedef struct {
        int SM;  // 0xMm (hexidecimal notation), M = SM Major version,
        // and m = SM minor version
        int Cores;
    } sSMtoCores;

    sSMtoCores nGpuArchCoresPerSM[] = {
        {0x30, 192},  // Kepler Generation (SM 3.0) GK10x class
        {0x32, 192},  // Kepler Generation (SM 3.2) GK10x class
        {0x35, 192},  // Kepler Generation (SM 3.5) GK11x class
        {0x37, 192},  // Kepler Generation (SM 3.7) GK21x class
        {0x50, 128},  // Maxwell Generation (SM 5.0) GM10x class
        {0x52, 128},  // Maxwell Generation (SM 5.2) GM20x class
        {0x53, 128},  // Maxwell Generation (SM 5.3) GM20x class
        {0x60, 64},   // Pascal Generation (SM 6.0) GP100 class
        {0x61, 128},  // Pascal Generation (SM 6.1) GP10x class
        {0x62, 128},  // Pascal Generation (SM 6.2) GP10x class
        {0x70, 64},   // Volta Generation (SM 7.0) GV100 class
        {0x72, 64},   // Volta Generation (SM 7.2) GV11b class
        {0x75, 64},   // Turing Generation (SM 7.5) TU1xx class
        {-1, -1}};

    int index = 0;

    while (nGpuArchCoresPerSM[index].SM != -1) {
        if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor)) {
            return nGpuArchCoresPerSM[index].Cores;
        }
        index++;
    }

#if GKO_VERBOSE_LEVEL >= 1
    // If we don't find the values, we use the last valid value by default
    // to allow proper execution
    std::cerr << "MapSMtoCores for SM " << major << "." << minor
              << "is undefined. The default value of "
              << nGpuArchCoresPerSM[index - 1].Cores << " Cores/SM is used."
              << std::endl;
#endif
    return nGpuArchCoresPerSM[index - 1].Cores;
}


}  // namespace


std::shared_ptr<HipExecutor> HipExecutor::create(
    int device_id, std::shared_ptr<Executor> master)
{
    return std::shared_ptr<HipExecutor>(
        new HipExecutor(device_id, std::move(master)),
        [device_id](HipExecutor *exec) {
            delete exec;
            if (!HipExecutor::get_num_execs(device_id)) {
                device_guard g(device_id);
                hipDeviceReset();
            }
        });
}


void OmpExecutor::raw_copy_to(const HipExecutor *dest, size_type num_bytes,
                              const void *src_ptr, void *dest_ptr) const
{
    device_guard g(dest->get_device_id());
    GKO_ASSERT_NO_HIP_ERRORS(
        hipMemcpy(dest_ptr, src_ptr, num_bytes, hipMemcpyHostToDevice));
}


void HipExecutor::raw_free(void *ptr) const noexcept
{
    device_guard g(this->get_device_id());
    auto error_code = hipFree(ptr);
    if (error_code != hipSuccess) {
#if GKO_VERBOSE_LEVEL >= 1
        // Unfortunately, if memory free fails, there's not much we can do
        std::cerr << "Unrecoverable HIP error on device " << this->device_id_
                  << " in " << __func__ << ": " << hipGetErrorName(error_code)
                  << ": " << hipGetErrorString(error_code) << std::endl
                  << "Exiting program" << std::endl;
#endif
        std::exit(error_code);
    }
}


void *HipExecutor::raw_alloc(size_type num_bytes) const
{
    void *dev_ptr = nullptr;
    device_guard g(this->get_device_id());
    auto error_code = hipMalloc(&dev_ptr, num_bytes);
    if (error_code != hipErrorMemoryAllocation) {
        GKO_ASSERT_NO_HIP_ERRORS(error_code);
    }
    GKO_ENSURE_ALLOCATED(dev_ptr, "hip", num_bytes);
    return dev_ptr;
}


void HipExecutor::raw_copy_to(const OmpExecutor *, size_type num_bytes,
                              const void *src_ptr, void *dest_ptr) const
{
    device_guard g(this->get_device_id());
    GKO_ASSERT_NO_HIP_ERRORS(
        hipMemcpy(dest_ptr, src_ptr, num_bytes, hipMemcpyDeviceToHost));
}


void HipExecutor::raw_copy_to(const CudaExecutor *src, size_type num_bytes,
                              const void *src_ptr, void *dest_ptr) const
{
#if GINKGO_HIP_PLATFORM_NVCC == 1
    device_guard g(this->get_device_id());
    GKO_ASSERT_NO_HIP_ERRORS(hipMemcpyPeer(dest_ptr, this->device_id_, src_ptr,
                                           src->get_device_id(), num_bytes));
#else
    GKO_NOT_SUPPORTED(HipExecutor);
#endif
}


void HipExecutor::raw_copy_to(const HipExecutor *src, size_type num_bytes,
                              const void *src_ptr, void *dest_ptr) const
{
    device_guard g(this->get_device_id());
    GKO_ASSERT_NO_HIP_ERRORS(hipMemcpyPeer(dest_ptr, this->device_id_, src_ptr,
                                           src->get_device_id(), num_bytes));
}


void HipExecutor::synchronize() const
{
    device_guard g(this->get_device_id());
    GKO_ASSERT_NO_HIP_ERRORS(hipDeviceSynchronize());
}


void HipExecutor::run(const Operation &op) const
{
    this->template log<log::Logger::operation_launched>(this, &op);
    device_guard g(this->get_device_id());
    op.run(
        std::static_pointer_cast<const HipExecutor>(this->shared_from_this()));
    this->template log<log::Logger::operation_completed>(this, &op);
}


int HipExecutor::get_num_devices()
{
    int deviceCount = 0;
    auto error_code = hipGetDeviceCount(&deviceCount);
    if (error_code == hipErrorNoDevice) {
        return 0;
    }
    GKO_ASSERT_NO_HIP_ERRORS(error_code);
    return deviceCount;
}


// TODO: Fix this
void HipExecutor::set_gpu_property()
{
    if (device_id_ < this->get_num_devices() && device_id_ >= 0) {
        device_guard g(this->get_device_id());
        GKO_ASSERT_NO_HIP_ERRORS(hipDeviceGetAttribute(
            &major_, hipDeviceAttributeComputeCapabilityMajor, device_id_));
        GKO_ASSERT_NO_HIP_ERRORS(hipDeviceGetAttribute(
            &minor_, hipDeviceAttributeComputeCapabilityMinor, device_id_));
        GKO_ASSERT_NO_HIP_ERRORS(hipDeviceGetAttribute(
            &num_multiprocessor_, hipDeviceAttributeMultiprocessorCount,
            device_id_));
        num_cores_per_sm_ = convert_sm_ver_to_cores(major_, minor_);
    }
}


void HipExecutor::init_handles()
{
    if (device_id_ < this->get_num_devices() && device_id_ >= 0) {
        const auto id = this->get_device_id();
        device_guard g(id);
        this->hipblas_handle_ = handle_manager<hipblasContext>(
            kernels::hip::hipblas::init(), [id](hipblasContext *handle) {
                device_guard g(id);
                kernels::hip::hipblas::destroy_hipblas_handle(handle);
            });
        this->hipsparse_handle_ = handle_manager<hipsparseContext>(
            kernels::hip::hipsparse::init(), [id](hipsparseContext *handle) {
                device_guard g(id);
                kernels::hip::hipsparse::destroy_hipsparse_handle(handle);
            });
    }
}


}  // namespace gko
