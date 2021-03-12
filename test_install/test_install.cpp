/*******************************<GINKGO LICENSE>******************************
Copyright (c) 2017-2021, the Ginkgo authors
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

#include <ginkgo/ginkgo.hpp>


#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


void assert_similar_matrices(const gko::matrix::Dense<> *m1,
                             const gko::matrix::Dense<> *m2, double prec)
{
    assert(m1->get_size()[0] == m2->get_size()[0]);
    assert(m1->get_size()[1] == m2->get_size()[1]);
    for (gko::size_type i = 0; i < m1->get_size()[0]; ++i) {
        for (gko::size_type j = 0; j < m2->get_size()[1]; ++j) {
            assert(std::abs(m1->at(i, j) - m2->at(i, j)) < prec);
        }
    }
}


template <typename Mtx>
void check_spmv(std::shared_ptr<gko::Executor> exec,
                gko::matrix_data<double> &A_raw, gko::matrix::Dense<> *b,
                gko::matrix::Dense<> *x)
{
    auto test = Mtx::create(exec);
#if HAS_REFERENCE
    auto x_clone = gko::clone(x);
    test->read(A_raw);
    test->apply(b, gko::lend(x_clone));

#if defined(HAS_HIP) || defined(HAS_CUDA)
    auto test_ref = Mtx::create(exec->get_master());
    auto x_ref = gko::clone(exec->get_master(), x);
    test_ref->read(A_raw);
    test_ref->apply(b, gko::lend(x_ref));

    auto x_clone_ref = gko::clone(exec->get_master(), gko::lend(x_clone));
    assert_similar_matrices(gko::lend(x_clone_ref), gko::lend(x_ref), 1e-14);
#endif  // defined(HAS_HIP) || defined(HAS_CUDA)
#endif  // HAS_REFERENCE
}


template <typename Solver>
void check_solver(std::shared_ptr<gko::Executor> exec,
                  gko::matrix_data<double> &A_raw, gko::matrix::Dense<> *b,
                  gko::matrix::Dense<> *x)
{
    using Mtx = gko::matrix::Csr<>;
    auto A =
        gko::share(Mtx::create(exec, std::make_shared<Mtx::load_balance>()));
#if HAS_REFERENCE
    A->read(A_raw);
#endif  // HAS_REFERENCE

    auto num_iters = 20u;
    double reduction_factor = 1e-7;
    auto solver_gen =
        Solver::build()
            .with_criteria(
                gko::stop::Iteration::build().with_max_iters(num_iters).on(
                    exec),
                gko::stop::ResidualNorm<>::build()
                    .with_reduction_factor(reduction_factor)
                    .on(exec))
            .on(exec);
#if HAS_REFERENCE
    auto x_clone = gko::clone(x);
    solver_gen->generate(A)->apply(b, gko::lend(x_clone));

#if defined(HAS_HIP) || defined(HAS_CUDA)
    auto A_ref =
        gko::share(Mtx::create(exec, std::make_shared<Mtx::load_balance>()));
    A_ref->read(A_raw);
    auto refExec = exec->get_master();
    auto solver_gen_ref =
        Solver::build()
            .with_criteria(
                gko::stop::Iteration::build().with_max_iters(num_iters).on(
                    exec),
                gko::stop::ResidualNorm<>::build()
                    .with_reduction_factor(reduction_factor)
                    .on(refExec))
            .on(refExec);
    auto x_ref = gko::clone(exec->get_master(), x);
    solver_gen->generate(A_ref)->apply(b, gko::lend(x_ref));

    auto x_clone_ref = gko::clone(exec->get_master(), gko::lend(x_clone));
    assert_similar_matrices(gko::lend(x_clone_ref), gko::lend(x_ref), 1e-12);
#endif  // defined(HAS_HIP) || defined(HAS_CUDA)
#endif  // HAS_REFERENCE
}


// core/base/polymorphic_object.hpp
class PolymorphicObjectTest : public gko::PolymorphicObject {};


int main(int, char **)
{
#if defined(HAS_CUDA)
    auto exec = gko::CudaExecutor::create(0, gko::ReferenceExecutor::create());
#elif defined(HAS_HIP)
    auto exec = gko::HipExecutor::create(0, gko::ReferenceExecutor::create());
#else
    auto exec = gko::ReferenceExecutor::create();
#endif

    using vec = gko::matrix::Dense<>;
    auto b = gko::read<vec>(std::ifstream("data/b.mtx"), exec);
    auto x = gko::read<vec>(std::ifstream("data/x0.mtx"), exec);
    std::ifstream A_file("data/A.mtx");
    auto A_raw = gko::read_raw<double>(A_file);

    // core/base/abstract_factory.hpp
    {
        using type1 = int;
        using type2 = double;
        static_assert(
            std::is_same<
                gko::AbstractFactory<type1, type2>::abstract_product_type,
                type1>::value,
            "abstract_factory.hpp not included properly!");
    }

    // core/base/array.hpp
    {
        using type1 = int;
        using ArrayType = gko::Array<type1>;
        ArrayType test;
    }

    // core/base/combination.hpp
    {
        using type1 = int;
        static_assert(
            std::is_same<gko::Combination<type1>::value_type, type1>::value,
            "combination.hpp not included properly!");
    }

    // core/base/composition.hpp
    {
        using type1 = int;
        static_assert(
            std::is_same<gko::Composition<type1>::value_type, type1>::value,
            "composition.hpp not included properly");
    }

    // core/base/dim.hpp
    {
        using type1 = int;
        auto test = gko::dim<3, type1>{4, 4, 4};
    }

    // core/base/exception.hpp
    {
        auto test = gko::Error(std::string("file"), 12,
                               std::string("Test for an error class."));
    }

    // core/base/exception_helpers.hpp
    {
        auto test = gko::dim<2>{3};
        GKO_ASSERT_IS_SQUARE_MATRIX(test);
    }

    // core/base/executor.hpp
    {
        auto test = gko::ReferenceExecutor::create();
    }

    // core/base/math.hpp
    {
        using testType = double;
        static_assert(gko::is_complex<testType>() == false,
                      "math.hpp not included properly!");
    }

    // core/base/matrix_data.hpp
    {
        gko::matrix_data<> test{};
    }

    // core/base/mtx_io.hpp
    {
        auto test = gko::layout_type::array;
    }

    // core/base/name_demangling.hpp
    {
        auto testVar = 3.0;
        auto test = gko::name_demangling::get_static_type(testVar);
    }

    // core/base/polymorphic_object.hpp
    {
        auto test = gko::layout_type::array;
    }

    // core/base/range.hpp
    {
        auto test = gko::span{12};
    }

    // core/base/range_accessors.hpp
    {
        auto testVar = 12;
        auto test = gko::range<gko::accessor::row_major<decltype(testVar), 2>>(
            &testVar, 1u, 1u, 1u);
    }

    // core/base/perturbation.hpp
    {
        using type1 = int;
        static_assert(
            std::is_same<gko::Perturbation<type1>::value_type, type1>::value,
            "perturbation.hpp not included properly");
    }

    // core/base/std_extensions.hpp
    {
        static_assert(std::is_same<gko::xstd::void_t<double>, void>::value,
                      "std::extensions.hpp not included properly!");
    }

    // core/base/types.hpp
    {
        gko::size_type test{12};
    }

    // core/base/utils.hpp
    {
        auto test = gko::null_deleter<double>{};
    }

    // core/base/version.hpp
    {
        auto test = gko::version_info::get().header_version;
    }

    // core/factorization/par_ilu.hpp
    {
        auto test = gko::factorization::ParIlu<>::build().on(exec);
    }

    // core/log/convergence.hpp
    {
        auto test = gko::log::Convergence<>::create(exec);
    }

    // core/log/record.hpp
    {
        auto test = gko::log::executor_data{};
    }

    // core/log/stream.hpp
    {
        auto test = gko::log::Stream<>::create(exec);
    }

#if GKO_HAVE_PAPI_SDE
    // core/log/papi.hpp
    {
        auto test = gko::log::Papi<>::create(exec);
    }
#endif  // GKO_HAVE_PAPI_SDE

    // core/matrix/coo.hpp
    {
        using Mtx = gko::matrix::Coo<>;
        check_spmv<Mtx>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/matrix/csr.hpp
    {
        using Mtx = gko::matrix::Csr<>;
        auto test = Mtx::create(exec, std::make_shared<Mtx::load_balance>());
    }

    // core/matrix/dense.hpp
    {
        using Mtx = gko::matrix::Dense<>;
        check_spmv<Mtx>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/matrix/ell.hpp
    {
        using Mtx = gko::matrix::Ell<>;
        check_spmv<Mtx>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/matrix/hybrid.hpp
    {
        using Mtx = gko::matrix::Hybrid<>;
        check_spmv<Mtx>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/matrix/identity.hpp
    {
        using Mtx = gko::matrix::Identity<>;
        auto test = Mtx::create(exec);
    }

    // core/matrix/permutation.hpp
    {
        using Mtx = gko::matrix::Permutation<>;
        auto test = Mtx::create(exec, gko::dim<2>{2, 2});
    }

    // core/matrix/sellp.hpp
    {
        using Mtx = gko::matrix::Sellp<>;
        check_spmv<Mtx>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/matrix/sparsity_csr.hpp
    {
        using Mtx = gko::matrix::SparsityCsr<>;
        auto test = Mtx::create(exec, gko::dim<2>{2, 2});
    }

    // core/multigrid/amgx_pgm.hpp
    {
        auto test = gko::multigrid::AmgxPgm<>::build().on(exec);
    }

    // core/preconditioner/ilu.hpp
    {
        auto test = gko::preconditioner::Ilu<>::build().on(exec);
    }

    // core/preconditioner/isai.hpp
    {
        auto test_l = gko::preconditioner::LowerIsai<>::build().on(exec);
        auto test_u = gko::preconditioner::UpperIsai<>::build().on(exec);
    }

    // core/preconditioner/jacobi.hpp
    {
        using Bj = gko::preconditioner::Jacobi<>;
        auto test = Bj::build().with_max_block_size(1u).on(exec);
    }

    // core/solver/bicgstab.hpp
    {
        using Solver = gko::solver::Bicgstab<>;
        check_solver<Solver>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/solver/cb_gmres.hpp
    {
        using Solver = gko::solver::CbGmres<>;
        check_solver<Solver>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/solver/cg.hpp
    {
        using Solver = gko::solver::Cg<>;
        check_solver<Solver>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/solver/cgs.hpp
    {
        using Solver = gko::solver::Cgs<>;
        check_solver<Solver>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/solver/fcg.hpp
    {
        using Solver = gko::solver::Fcg<>;
        check_solver<Solver>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/solver/gmres.hpp
    {
        using Solver = gko::solver::Gmres<>;
        check_solver<Solver>(exec, A_raw, gko::lend(b), gko::lend(x));
    }

    // core/solver/ir.hpp
    {
        using Solver = gko::solver::Ir<>;
        auto test =
            Solver::build()
                .with_criteria(
                    gko::stop::Iteration::build().with_max_iters(1u).on(exec))
                .on(exec);
    }

    // core/solver/lower_trs.hpp
    {
        using Solver = gko::solver::LowerTrs<>;
        auto test = Solver::build().on(exec);
    }

    // core/stop/
    {
        // iteration.hpp
        auto iteration =
            gko::stop::Iteration::build().with_max_iters(1u).on(exec);

        // time.hpp
        auto time = gko::stop::Time::build()
                        .with_time_limit(std::chrono::milliseconds(10))
                        .on(exec);

        // residual_norm.hpp
        auto main_res = gko::stop::ResidualNorm<>::build()
                            .with_reduction_factor(1e-10)
                            .with_baseline(gko::stop::mode::absolute)
                            .on(exec);

        auto implicit_res = gko::stop::ImplicitResidualNorm<>::build()
                                .with_reduction_factor(1e-10)
                                .with_baseline(gko::stop::mode::absolute)
                                .on(exec);

        auto res_red = gko::stop::ResidualNormReduction<>::build()
                           .with_reduction_factor(1e-10)
                           .on(exec);

        auto rel_res =
            gko::stop::RelativeResidualNorm<>::build().with_tolerance(1e-10).on(
                exec);

        auto abs_res =
            gko::stop::AbsoluteResidualNorm<>::build().with_tolerance(1e-10).on(
                exec);

        // stopping_status.hpp
        auto stop_status = gko::stopping_status{};

        // combined.hpp
        auto combined =
            gko::stop::Combined::build()
                .with_criteria(std::move(time), std::move(iteration))
                .on(exec);
    }
#if defined(HAS_CUDA)
    auto extra_info = "(CUDA)";
#elif defined(HAS_HIP)
    auto extra_info = "(HIP)";
#else
    auto extra_info = "";
#endif
    std::cout << "test_install" << extra_info
              << ": the Ginkgo installation was correctly detected "
                 "and is complete."
              << std::endl;

    return 0;
}
