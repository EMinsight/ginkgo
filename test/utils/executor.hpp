/*******************************<GINKGO LICENSE>******************************
Copyright (c) 2017-2022, the Ginkgo authors
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

#ifndef GKO_TEST_UTILS_EXECUTOR_HPP_
#define GKO_TEST_UTILS_EXECUTOR_HPP_


#include <ginkgo/core/base/executor.hpp>


#include <memory>
#include <stdexcept>


#include <gtest/gtest.h>


#if GINKGO_COMMON_SINGLE_MODE
#define SKIP_IF_SINGLE_MODE GTEST_SKIP() << "Skip due to single mode"
#else
#define SKIP_IF_SINGLE_MODE                                                  \
    static_assert(true,                                                      \
                  "This assert is used to counter the false positive extra " \
                  "semi-colon warnings")
#endif


inline std::shared_ptr<gko::ReferenceExecutor> init_executor(
    std::shared_ptr<gko::ReferenceExecutor>, gko::ReferenceExecutor*)
{
    return gko::ReferenceExecutor::create();
}


inline std::shared_ptr<gko::OmpExecutor> init_executor(
    std::shared_ptr<gko::ReferenceExecutor>, gko::OmpExecutor*)
{
    return gko::OmpExecutor::create();
}


inline std::shared_ptr<gko::CudaExecutor> init_executor(
    std::shared_ptr<gko::ReferenceExecutor> ref, gko::CudaExecutor*)
{
    {
        if (gko::CudaExecutor::get_num_devices() == 0) {
            throw std::runtime_error{"No suitable HIP devices"};
        }
        return gko::CudaExecutor::create(0, ref);
    }
}


inline std::shared_ptr<gko::HipExecutor> init_executor(
    std::shared_ptr<gko::ReferenceExecutor> ref, gko::HipExecutor*)
{
    if (gko::HipExecutor::get_num_devices() == 0) {
        throw std::runtime_error{"No suitable HIP devices"};
    }
    return gko::HipExecutor::create(0, ref);
}


inline std::shared_ptr<gko::DpcppExecutor> init_executor(
    std::shared_ptr<gko::ReferenceExecutor> ref, gko::DpcppExecutor*)
{
    if (gko::DpcppExecutor::get_num_devices("gpu") > 0) {
        return gko::DpcppExecutor::create(0, ref, "gpu");
    } else if (gko::DpcppExecutor::get_num_devices("cpu") > 0) {
        return gko::DpcppExecutor::create(0, ref, "cpu");
    } else {
        throw std::runtime_error{"No suitable DPC++ devices"};
    }
}


class CommonTestFixture : public ::testing::Test {
public:
    CommonTestFixture()
        : ref{gko::ReferenceExecutor::create()},
          exec{init_executor(ref, static_cast<gko::EXEC_TYPE*>(nullptr))}
    {}

    void TearDown() final
    {
        if (exec != nullptr) {
            ASSERT_NO_THROW(exec->synchronize());
        }
    }

    std::shared_ptr<gko::ReferenceExecutor> ref;
    std::shared_ptr<gko::EXEC_TYPE> exec;
};


#endif  // GKO_TEST_UTILS_EXECUTOR_HPP_
