// SPDX-FileCopyrightText: 2024 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

template <typename ValueType>
inline void extract_block_pattern_impl(
    const size_type k, const matrix::Csr<ValueType, int>* const first_sys_csr,
    const int* const cumulative_block_storage, const int* const block_pointers,
    int* const blocks_pattern)
{
    const int* row_ptrs = first_sys_csr->get_const_row_ptrs();
    const int* col_idxs = first_sys_csr->get_const_col_idxs();

    const int row_idx_st = block_pointers[k];
    const int row_idx_end = block_pointers[k + 1];  // exclusive

    const auto offset =
        detail::batch_jacobi::get_block_offset(k, cumulative_block_storage);
    const auto stride = detail::batch_jacobi::get_stride(k, block_pointers);

    // pattern is stored in row-major order
    for (int row_idx = row_idx_st; row_idx < row_idx_end; row_idx++) {
        for (int i = row_ptrs[row_idx]; i < row_ptrs[row_idx + 1]; i++) {
            const auto col_idx = col_idxs[i];

            if (col_idx >= row_idx_st && col_idx < row_idx_end) {
                // (row_idx, col_idx) is the part of the diagonal block
                // storing the position/index of this element at place: (row_idx
                // - row_idx_st, col_idx - row_idx_st)
                const auto dense_block_row = row_idx - row_idx_st;
                const auto dense_block_col = col_idx - row_idx_st;

                blocks_pattern[offset + dense_block_row * stride +
                               dense_block_col] = i;
            }
        }
    }
}


template <typename ValueType>
inline void swap_rows(const int size, std::vector<ValueType>& mat,
                      const int row1, const int row2)
{
    for (int c = 0; c < size; c++) {
        std::swap(mat[row1 * size + c], mat[row2 * size + c]);
    }
}


template <typename ValueType>
inline int choose_pivot(const int k, const int block_size,
                        const std::vector<ValueType>& temp_dense_block)
{
    int piv = k;
    ValueType piv_val = temp_dense_block[k * block_size + k];
    for (int r = k + 1; r < block_size; r++)  // uncoalesced accesses
    {
        ValueType curr_val = temp_dense_block[r * block_size + k];
        if (abs(piv_val) < abs(curr_val)) {
            piv = r;
            piv_val = curr_val;
        }
    }
    return piv;
}


template <typename ValueType>
inline void invert_dense_block(const int block_size,
                               std::vector<ValueType>& temp_dense_block,
                               std::vector<int>& col_perm)
{
    // Gauss-Jordan elimination with pivoting
    // the block is stored in the row-major order, with row stride = block_size
    for (int k = 0; k < block_size; k++) {
        const int piv = choose_pivot(k, block_size, temp_dense_block);
        if (k != piv) {
            swap_rows(block_size, temp_dense_block, k, piv);
            std::swap(col_perm[piv], col_perm[k]);
        }
        const ValueType diag = temp_dense_block[k * block_size + k];

        // (scaling the kth col)
        // uncoalesced accesses
        for (int r = 0; r < block_size; r++) {
            temp_dense_block[r * block_size + k] /= -diag;
        }
        temp_dense_block[k * block_size + k] = zero<ValueType>();

        // rank-1 update (Note: the kth row and kth col do not change)
        // data-locality
        for (int r = 0; r < block_size; r++) {
            for (int c = 0; c < block_size; c++) {
                temp_dense_block[r * block_size + c] +=
                    temp_dense_block[r * block_size + k] *
                    temp_dense_block[k * block_size + c];
            }
        }

        // (scaling the kth row)
        // data-locality
        for (int c = 0; c < block_size; c++) {
            temp_dense_block[k * block_size + c] /= diag;
        }
        temp_dense_block[k * block_size + k] = one<ValueType>() / diag;
    }
}


template <typename ValueType>
inline void compute_block_jacobi_impl(
    const size_type batch_idx, const size_type block_idx,
    const batch::matrix::csr::batch_item<const ValueType, const int>& A_entry,
    const size_type num_blocks, const int* const cumulative_block_storage,
    const int* const block_pointers, const int* const blocks_pattern,
    ValueType* const blocks)
{
    const auto offset_batch = detail::batch_jacobi::get_batch_offset(
        batch_idx, num_blocks, cumulative_block_storage);
    const auto offset_indiv = detail::batch_jacobi::get_block_offset(
        block_idx, cumulative_block_storage);
    const auto stride =
        detail::batch_jacobi::get_stride(block_idx, block_pointers);
    // the blocks are in row-major order
    ValueType* dense_block_ptr = blocks + offset_batch + offset_indiv;

    const auto bsize =
        block_pointers[block_idx + 1] - block_pointers[block_idx];

    // also in row-major order
    std::vector<ValueType> temp_dense_block(bsize * bsize);
    std::vector<int> col_perm(bsize);
    for (int i = 0; i < bsize; i++) {
        col_perm[i] = i;
    }

    for (int row = 0; row < bsize; row++) {
        for (int col = 0; col < bsize; col++) {
            auto to_write = zero<ValueType>();
            // data locality
            const auto idx =
                blocks_pattern[offset_indiv +
                               row * detail::batch_jacobi::get_stride(
                                         block_idx, block_pointers) +
                               col];
            if (idx >= 0) {
                assert(idx < A_entry.get_single_item_num_nnz());
                to_write = A_entry.values[idx];
            }
            // data locality
            temp_dense_block[row * bsize + col] = to_write;
        }
    }

    // invert the diagonal block
    invert_dense_block(bsize, temp_dense_block, col_perm);

    // store the dense block (take into account the col permutations)
    for (int row = 0; row < bsize; row++) {
        for (int col = 0; col < bsize; col++) {
            dense_block_ptr[row * stride + col_perm[col]] =
                temp_dense_block[row * bsize + col];
        }
    }
}
