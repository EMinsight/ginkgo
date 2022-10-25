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

#include <gtest/gtest.h>


#include <ginkgo/config.hpp>
#include <ginkgo/core/distributed/matrix.hpp>
#include <ginkgo/core/distributed/partition.hpp>
#include <ginkgo/core/distributed/vector.hpp>
#include <ginkgo/core/matrix/coo.hpp>
#include <ginkgo/core/matrix/csr.hpp>
#include <ginkgo/core/matrix/dense.hpp>
#include <ginkgo/core/matrix/ell.hpp>
#include <ginkgo/core/matrix/fbcsr.hpp>
#include <ginkgo/core/matrix/hybrid.hpp>
#include <ginkgo/core/matrix/sellp.hpp>


#include "core/test/utils.hpp"


namespace {


using comm_index_type = gko::experimental::distributed::comm_index_type;


template <typename ValueType, typename IndexType>
class CustomLinOp
    : public gko::EnableLinOp<CustomLinOp<ValueType, IndexType>>,
      public gko::ReadableFromMatrixData<ValueType, IndexType>,
      public gko::EnableCreateMethod<CustomLinOp<ValueType, IndexType>> {
public:
    void read(const gko::matrix_data<ValueType, IndexType>& data) override {}

    explicit CustomLinOp(std::shared_ptr<const gko::Executor> exec)
        : gko::EnableLinOp<CustomLinOp>(exec)
    {}

protected:
    void apply_impl(const gko::LinOp* b, gko::LinOp* x) const override {}

    void apply_impl(const gko::LinOp* alpha, const gko::LinOp* b,
                    const gko::LinOp* beta, gko::LinOp* x) const override
    {}
};


template <typename ValueLocalGlobalIndexType>
class MatrixBuilder : public ::testing::Test {
protected:
    using value_type = typename std::tuple_element<
        0, decltype(ValueLocalGlobalIndexType())>::type;
    using local_index_type = typename std::tuple_element<
        1, decltype(ValueLocalGlobalIndexType())>::type;
    using global_index_type = typename std::tuple_element<
        2, decltype(ValueLocalGlobalIndexType())>::type;
    using dist_mtx_type =
        gko::experimental::distributed::Matrix<value_type, local_index_type,
                                               global_index_type>;
    using dist_vec_type = gko::experimental::distributed::Vector<value_type>;

    MatrixBuilder()
        : ref(gko::ReferenceExecutor::create()),
          comm(gko::mpi::communicator(MPI_COMM_WORLD))
    {}

    void SetUp() override {}

    template <typename F>
    void forall_matrix_types(F&& f)
    {
        using namespace gko::matrix;
        auto empty_test = [](const gko::LinOp*) {};
        {
            SCOPED_TRACE("With Coo");
            f(gko::with_matrix_type<Coo>(),
              Coo<value_type, local_index_type>::create(this->ref), empty_test);
        }
        {
            SCOPED_TRACE("With Csr");
            f(gko::with_matrix_type<Csr>(),
              Csr<value_type, local_index_type>::create(this->ref), empty_test);
        }
        {
            SCOPED_TRACE("With Csr with strategy");
            using ConcreteCsr = Csr<value_type, local_index_type>;
            f(gko::with_matrix_type<Csr>(
                  std::make_shared<typename ConcreteCsr::classical>()),
              ConcreteCsr::create(this->ref), [](const gko::LinOp* local_mat) {
                  auto local_csr = gko::as<ConcreteCsr>(local_mat);

                  ASSERT_NO_THROW(gko::as<typename ConcreteCsr::classical>(
                      local_csr->get_strategy()));
              });
        }
        {
            SCOPED_TRACE("With Ell");
            f(gko::with_matrix_type<Ell>(),
              Ell<value_type, local_index_type>::create(this->ref), empty_test);
        }
        {
            SCOPED_TRACE("With Fbcsr");
            f(gko::with_matrix_type<Fbcsr>(),
              Fbcsr<value_type, local_index_type>::create(this->ref),
              empty_test);
        }
        {
            SCOPED_TRACE("With Fbcsr with block_size");
            f(gko::with_matrix_type<Fbcsr>(5),
              Fbcsr<value_type, local_index_type>::create(this->ref),
              [](const gko::LinOp* local_mat) {
                  auto local_fbcsr =
                      gko::as<Fbcsr<value_type, local_index_type>>(local_mat);

                  ASSERT_EQ(local_fbcsr->get_block_size(), 5);
              });
        }
        {
            SCOPED_TRACE("With Hybrid");
            f(gko::with_matrix_type<Hybrid>(),
              Hybrid<value_type, local_index_type>::create(this->ref),
              empty_test);
        }
        {
            SCOPED_TRACE("With Hybrid with strategy");
            using Concrete = Hybrid<value_type, local_index_type>;
            f(gko::with_matrix_type<Hybrid>(
                  std::make_shared<typename Concrete::column_limit>(11)),
              Concrete::create(this->ref), [](const gko::LinOp* local_mat) {
                  auto local_hy = gko::as<Concrete>(local_mat);

                  ASSERT_NO_THROW(gko::as<typename Concrete::column_limit>(
                      local_hy->get_strategy()));
                  ASSERT_EQ(gko::as<typename Concrete::column_limit>(
                                local_hy->get_strategy())
                                ->get_num_columns(),
                            11);
              });
        }
        {
            SCOPED_TRACE("With Sellp");
            f(gko::with_matrix_type<Sellp>(),
              Sellp<value_type, local_index_type>::create(this->ref),
              empty_test);
        }
    }

    template <typename LocalMatrixType, typename NonLocalMatrixType>
    void expected_interface_no_throw(dist_mtx_type* mat,
                                     LocalMatrixType local_matrix_type,
                                     NonLocalMatrixType non_local_matrix_type)
    {
        auto num_rows = mat->get_size()[0];
        auto a = dist_vec_type::create(ref, comm);
        auto b = dist_vec_type::create(ref, comm);
        auto convert_result = dist_mtx_type::create(
            ref, comm, local_matrix_type, non_local_matrix_type);
        auto move_result = dist_mtx_type::create(ref, comm, local_matrix_type,
                                                 non_local_matrix_type);

        ASSERT_NO_THROW(mat->apply(a.get(), b.get()));
        ASSERT_NO_THROW(mat->convert_to(convert_result.get()));
        ASSERT_NO_THROW(mat->move_to(move_result.get()));
    }


    std::shared_ptr<const gko::ReferenceExecutor> ref;
    gko::mpi::communicator comm;
};

TYPED_TEST_SUITE(MatrixBuilder, gko::test::ValueLocalGlobalIndexTypes);


TYPED_TEST(MatrixBuilder, BuildWithLocal)
{
    using value_type = typename TestFixture::value_type;
    using index_type = typename TestFixture::local_index_type;
    using dist_mat_type = typename TestFixture::dist_mtx_type;
    this->template forall_matrix_types([this](auto with_matrix_type,
                                              auto expected_type_ptr,
                                              auto additional_test) {
        using expected_type = typename std::remove_pointer<
            decltype(expected_type_ptr.get())>::type;

        auto mat =
            dist_mat_type ::create(this->ref, this->comm, with_matrix_type);

        ASSERT_NO_THROW(gko::as<expected_type>(mat->get_local_matrix()));
        additional_test(mat->get_local_matrix().get());
        additional_test(mat->get_non_local_matrix().get());
        this->expected_interface_no_throw(mat.get(), with_matrix_type,
                                          with_matrix_type);
    });
}


TYPED_TEST(MatrixBuilder, BuildWithLocalAndNonLocal)
{
    using value_type = typename TestFixture::value_type;
    using index_type = typename TestFixture::local_index_type;
    using dist_mat_type = typename TestFixture::dist_mtx_type;
    this->template forall_matrix_types([this](auto with_local_matrix_type,
                                              auto expected_local_type_ptr,
                                              auto additional_local_test) {
        using expected_local_type = typename std::remove_pointer<
            decltype(expected_local_type_ptr.get())>::type;
        this->forall_matrix_types([=](auto with_non_local_matrix_type,
                                      auto expected_non_local_type_ptr,
                                      auto additional_non_local_test) {
            using expected_non_local_type = typename std::remove_pointer<
                decltype(expected_non_local_type_ptr.get())>::type;

            auto mat = dist_mat_type ::create(this->ref, this->comm,
                                              with_local_matrix_type,
                                              with_non_local_matrix_type);

            ASSERT_NO_THROW(
                gko::as<expected_local_type>(mat->get_local_matrix()));
            ASSERT_NO_THROW(
                gko::as<expected_non_local_type>(mat->get_non_local_matrix()));
            additional_local_test(mat->get_local_matrix().get());
            additional_non_local_test(mat->get_non_local_matrix().get());
            this->expected_interface_no_throw(mat.get(), with_local_matrix_type,
                                              with_non_local_matrix_type);
        });
    });
}


TYPED_TEST(MatrixBuilder, BuildWithCustomLinOp)
{
    using value_type = typename TestFixture::value_type;
    using index_type = typename TestFixture::local_index_type;
    using dist_mat_type = typename TestFixture::dist_mtx_type;
    using custom_type = CustomLinOp<value_type, index_type>;

    auto mat = dist_mat_type::create(this->ref, this->comm,
                                     gko::with_matrix_type<CustomLinOp>());

    ASSERT_NO_THROW(gko::as<custom_type>(mat->get_local_matrix()));
    this->expected_interface_no_throw(mat.get(),
                                      gko::with_matrix_type<CustomLinOp>(),
                                      gko::with_matrix_type<CustomLinOp>());
}


}  // namespace
