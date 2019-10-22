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

#ifndef GKO_CORE_MEMORY_SPACE_HPP_
#define GKO_CORE_MEMORY_SPACE_HPP_


#include <memory>
#include <mutex>
#include <sstream>
#include <tuple>
#include <type_traits>


#include <ginkgo/core/base/types.hpp>
#include <ginkgo/core/log/logger.hpp>
#include <ginkgo/core/synthesizer/containers.hpp>


namespace gko {


class HostMemorySpace;
class CudaMemorySpace;

#define GKO_FORWARD_DECLARE(_type, ...) class _type

GKO_ENABLE_FOR_ALL_EXECUTORS(GKO_FORWARD_DECLARE);

#undef GKO_FORWARD_DECLARE


class ReferenceExecutor;


namespace detail {


template <typename>
class MemorySpaceBase;


}  // namespace detail


class MemorySpace : public log::EnableLogging<MemorySpace> {
    template <typename T>
    friend class detail::MemorySpaceBase;

public:
    virtual ~MemorySpace() = default;

    MemorySpace() = default;
    MemorySpace(MemorySpace &) = delete;
    MemorySpace(MemorySpace &&) = default;
    MemorySpace &operator=(MemorySpace &) = delete;
    MemorySpace &operator=(MemorySpace &&) = default;

    /**
     * Allocates memory in this MemorySpace.
     *
     * @tparam T  datatype to allocate
     *
     * @param num_elems  number of elements of type T to allocate
     *
     * @throw AllocationError  if the allocation failed
     *
     * @return pointer to allocated memory
     */
    template <typename T>
    T *alloc(size_type num_elems) const
    {
        this->template log<log::Logger::allocation_started>(
            this, num_elems * sizeof(T));
        T *allocated = static_cast<T *>(this->raw_alloc(num_elems * sizeof(T)));
        this->template log<log::Logger::allocation_completed>(
            this, num_elems * sizeof(T), reinterpret_cast<uintptr>(allocated));
        return allocated;
    }

    /**
     * Frees memory previously allocated with MemorySpace::alloc().
     *
     * If `ptr` is a `nullptr`, the function has no effect.
     *
     * @param ptr  pointer to the allocated memory block
     */
    void free(void *ptr) const noexcept
    {
        this->template log<log::Logger::free_started>(
            this, reinterpret_cast<uintptr>(ptr));
        this->raw_free(ptr);
        this->template log<log::Logger::free_completed>(
            this, reinterpret_cast<uintptr>(ptr));
    }

    /**
     * Copies data from another MemorySpace.
     *
     * @tparam T  datatype to copy
     *
     * @param src_exec  MemorySpace from which the memory will be copied
     * @param num_elems  number of elements of type T to copy
     * @param src_ptr  pointer to a block of memory containing the data to be
     *                 copied
     * @param dest_ptr  pointer to an allocated block of memory
     *                  where the data will be copied to
     */
    template <typename T>
    void copy_from(const MemorySpace *src_exec, size_type num_elems,
                   const T *src_ptr, T *dest_ptr) const
    {
        this->template log<log::Logger::copy_started>(
            src_exec, this, reinterpret_cast<uintptr>(src_ptr),
            reinterpret_cast<uintptr>(dest_ptr), num_elems * sizeof(T));
        this->raw_copy_from(src_exec, num_elems * sizeof(T), src_ptr, dest_ptr);
        this->template log<log::Logger::copy_completed>(
            src_exec, this, reinterpret_cast<uintptr>(src_ptr),
            reinterpret_cast<uintptr>(dest_ptr), num_elems * sizeof(T));
    }

protected:
    /**
     * Allocates raw memory in this MemorySpace.
     *
     * @param size  number of bytes to allocate
     *
     * @throw AllocationError  if the allocation failed
     *
     * @return raw pointer to allocated memory
     */
    virtual void *raw_alloc(size_type size) const = 0;

    /**
     * Frees memory previously allocated with MemorySpace::alloc().
     *
     * If `ptr` is a `nullptr`, the function has no effect.
     *
     * @param ptr  pointer to the allocated memory block
     */
    virtual void raw_free(void *ptr) const noexcept = 0;

    /**
     * Copies raw data from another MemorySpace.
     *
     * @param src_exec  MemorySpace from which the memory will be copied
     * @param n_bytes  number of bytes to copy
     * @param src_ptr  pointer to a block of memory containing the data to be
     *                 copied
     * @param dest_ptr  pointer to an allocated block of memory where the data
     *                  will be copied to
     */
    virtual void raw_copy_from(const MemorySpace *src_exec, size_type n_bytes,
                               const void *src_ptr, void *dest_ptr) const = 0;

/**
 * @internal
 * Declares a raw_copy_to() overload for a specified MemorySpace subclass.
 *
 * This is the second stage of the double dispatch emulation required to
 * implement raw_copy_from().
 *
 * @param _exec_type  the MemorySpace subclass
 */
#define GKO_ENABLE_RAW_COPY_TO(_exec_type, ...)                              \
    virtual void raw_copy_to(const _exec_type *dest_exec, size_type n_bytes, \
                             const void *src_ptr, void *dest_ptr) const = 0

    GKO_ENABLE_FOR_ALL_EXECUTORS(GKO_ENABLE_RAW_COPY_TO);

#undef GKO_ENABLE_RAW_COPY_TO
};


/**
 * This is a deleter that uses an executor's `free` method to deallocate the
 * data.
 *
 * @tparam T  the type of object being deleted
 *
 * @ingroup MemorySpace
 */
template <typename T>
class executor_deleter {
public:
    using pointer = T *;

    /**
     * Creates a new deleter.
     *
     * @param exec  the executor used to free the data
     */
    explicit executor_deleter(std::shared_ptr<const MemorySpace> exec)
        : exec_{exec}
    {}

    /**
     * Deletes the object.
     *
     * @param ptr  pointer to the object being deleted
     */
    void operator()(pointer ptr) const
    {
        if (exec_) {
            exec_->free(ptr);
        }
    }

private:
    std::shared_ptr<const MemorySpace> exec_;
};

// a specialization for arrays
template <typename T>
class executor_deleter<T[]> {
public:
    using pointer = T[];

    explicit executor_deleter(std::shared_ptr<const MemorySpace> exec)
        : exec_{exec}
    {}

    void operator()(pointer ptr) const
    {
        if (exec_) {
            exec_->free(ptr);
        }
    }

private:
    std::shared_ptr<const MemorySpace> exec_;
};


namespace detail {


template <typename ConcreteMemorySpace>
class MemorySpaceBase : public MemorySpace {
public:
    void run(const Operation &op) const override
    {
        this->template log<log::Logger::operation_launched>(this, &op);
        op.run(self()->shared_from_this());
        this->template log<log::Logger::operation_completed>(this, &op);
    }

protected:
    void raw_copy_from(const MemorySpace *src_exec, size_type n_bytes,
                       const void *src_ptr, void *dest_ptr) const override
    {
        src_exec->raw_copy_to(self(), n_bytes, src_ptr, dest_ptr);
    }

private:
    ConcreteMemorySpace *self() noexcept
    {
        return static_cast<ConcreteMemorySpace *>(this);
    }

    const ConcreteMemorySpace *self() const noexcept
    {
        return static_cast<const ConcreteMemorySpace *>(this);
    }
};


}  // namespace detail


#define GKO_OVERRIDE_RAW_COPY_TO(_executor_type, ...)                    \
    void raw_copy_to(const _executor_type *dest_exec, size_type n_bytes, \
                     const void *src_ptr, void *dest_ptr) const override


/**
 * This is the MemorySpace subclass which represents the OpenMP device
 * (typically CPU).
 *
 * @ingroup exec_omp
 * @ingroup MemorySpace
 */
class OmpMemorySpace : public detail::MemorySpaceBase<OmpMemorySpace>,
                       public std::enable_shared_from_this<OmpMemorySpace> {
    friend class detail::MemorySpaceBase<OmpMemorySpace>;

public:
    /**
     * Creates a new OmpMemorySpace.
     */
    static std::shared_ptr<OmpMemorySpace> create()
    {
        return std::shared_ptr<OmpMemorySpace>(new OmpMemorySpace());
    }

    std::shared_ptr<MemorySpace> get_master() noexcept override;

    std::shared_ptr<const MemorySpace> get_master() const noexcept override;

    void synchronize() const override;

protected:
    OmpMemorySpace() = default;

    void *raw_alloc(size_type size) const override;

    void raw_free(void *ptr) const noexcept override;

    GKO_ENABLE_FOR_ALL_EXECUTORS(GKO_OVERRIDE_RAW_COPY_TO);
};


namespace kernels {
namespace omp {
using DefaultMemorySpace = OmpMemorySpace;
}  // namespace omp
}  // namespace kernels


/**
 * This is a specialization of the OmpMemorySpace, which runs the reference
 * implementations of the kernels used for debugging purposes.
 *
 * @ingroup exec_ref
 * @ingroup MemorySpace
 */
class ReferenceMemorySpace : public OmpMemorySpace {
public:
    static std::shared_ptr<ReferenceMemorySpace> create()
    {
        return std::shared_ptr<ReferenceMemorySpace>(
            new ReferenceMemorySpace());
    }

    void run(const Operation &op) const override
    {
        this->template log<log::Logger::operation_launched>(this, &op);
        op.run(std::static_pointer_cast<const ReferenceMemorySpace>(
            this->shared_from_this()));
        this->template log<log::Logger::operation_completed>(this, &op);
    }

protected:
    ReferenceMemorySpace() = default;
};


namespace kernels {
namespace reference {
using DefaultMemorySpace = ReferenceMemorySpace;
}  // namespace reference
}  // namespace kernels


/**
 * This is the MemorySpace subclass which represents the CUDA device.
 *
 * @ingroup exec_cuda
 * @ingroup MemorySpace
 */
class CudaMemorySpace : public detail::MemorySpaceBase<CudaMemorySpace>,
                        public std::enable_shared_from_this<CudaMemorySpace> {
    friend class detail::MemorySpaceBase<CudaMemorySpace>;

public:
    /**
     * Creates a new CudaMemorySpace.
     *
     * @param device_id  the CUDA device id of this device
     * @param master  an executor on the host that is used to invoke the device
     * kernels
     */
    static std::shared_ptr<CudaMemorySpace> create(
        int device_id, std::shared_ptr<MemorySpace> master);

    ~CudaMemorySpace() { decrease_num_execs(this->device_id_); }

    std::shared_ptr<MemorySpace> get_master() noexcept override;

    std::shared_ptr<const MemorySpace> get_master() const noexcept override;

    void synchronize() const override;

    void run(const Operation &op) const override;

    /**
     * Get the CUDA device id of the device associated to this executor.
     */
    int get_device_id() const noexcept { return device_id_; }

    /**
     * Get the number of devices present on the system.
     */
    static int get_num_devices();

    /**
     * Get the number of cores per SM of this executor.
     */
    int get_num_cores_per_sm() const noexcept { return num_cores_per_sm_; }

    /**
     * Get the number of multiprocessor of this executor.
     */
    int get_num_multiprocessor() const noexcept { return num_multiprocessor_; }

    /**
     * Get the number of warps of this executor.
     */
    int get_num_warps() const noexcept
    {
        constexpr uint32 warp_size = 32;
        auto warps_per_sm = num_cores_per_sm_ / warp_size;
        return num_multiprocessor_ * warps_per_sm;
    }

    /**
     * Get the major verion of compute capability.
     */
    int get_major_version() const noexcept { return major_; }

    /**
     * Get the minor verion of compute capability.
     */
    int get_minor_version() const noexcept { return minor_; }

    /**
     * Get the cublas handle for this executor
     *
     * @return  the cublas handle (cublasContext*) for this executor
     */
    cublasContext *get_cublas_handle() const { return cublas_handle_.get(); }

    /**
     * Get the cusparse handle for this executor
     *
     * @return the cusparse handle (cusparseContext*) for this executor
     */
    cusparseContext *get_cusparse_handle() const
    {
        return cusparse_handle_.get();
    }

protected:
    void set_gpu_property();

    void init_handles();

    CudaMemorySpace(int device_id, std::shared_ptr<MemorySpace> master)
        : device_id_(device_id),
          master_(master),
          num_cores_per_sm_(0),
          num_multiprocessor_(0),
          major_(0),
          minor_(0)
    {
        assert(device_id < max_devices);
        this->set_gpu_property();
        this->init_handles();
        increase_num_execs(device_id);
    }

    void *raw_alloc(size_type size) const override;

    void raw_free(void *ptr) const noexcept override;

    GKO_ENABLE_FOR_ALL_EXECUTORS(GKO_OVERRIDE_RAW_COPY_TO);

    static void increase_num_execs(int device_id)
    {
        std::lock_guard<std::mutex> guard(mutex[device_id]);
        num_execs[device_id]++;
    }

    static void decrease_num_execs(int device_id)
    {
        std::lock_guard<std::mutex> guard(mutex[device_id]);
        num_execs[device_id]--;
    }

    static int get_num_execs(int device_id)
    {
        std::lock_guard<std::mutex> guard(mutex[device_id]);
        return num_execs[device_id];
    }

private:
    int device_id_;
    std::shared_ptr<MemorySpace> master_;
    int num_cores_per_sm_;
    int num_multiprocessor_;
    int major_;
    int minor_;

    template <typename T>
    using handle_manager = std::unique_ptr<T, std::function<void(T *)>>;
    handle_manager<cublasContext> cublas_handle_;
    handle_manager<cusparseContext> cusparse_handle_;

    static constexpr int max_devices = 64;
    static int num_execs[max_devices];
    static std::mutex mutex[max_devices];
};


namespace kernels {
namespace cuda {
using DefaultMemorySpace = CudaMemorySpace;
}  // namespace cuda
}  // namespace kernels


#undef GKO_OVERRIDE_RAW_COPY_TO


}  // namespace gko


#endif  // GKO_CORE_MEMORY_SPACE_HPP_
