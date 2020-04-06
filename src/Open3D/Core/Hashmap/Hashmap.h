// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <thrust/device_vector.h>
#include "Open3D/Core/CUDAUtils.h"
#include "Open3D/Core/MemoryManager.h"

#include "HashmapCUDA.h"

// template <typename Key, typename Value>
// struct Pair {
//     Key first;
//     Value second;
//     __device__ __host__ Pair() {}
//     __device__ __host__ Pair(const Key& key, const Value& value)
//         : first(key), second(value) {}
// };

// template <typename Key, typename Value>
// __device__ __host__ Pair<Key, Value> make_pair(const Key& key,
//                                                const Value& value) {
//     return Pair<Key, Value>(key, value);
// }

// template <typename Key, typename Value>
// using Iterator = Pair<Key, Value>*;

/*
 * Default hash function:
 * It treat any kind of input as a concatenation of ints.
 */
struct hash {
    __device__ __host__ hash() {}
    __device__ __host__ hash(uint32_t key_size) : key_size_(key_size) {}
    __device__ __host__ uint64_t operator()(uint8_t* key_ptr) const {
        uint64_t hash = UINT64_C(14695981039346656037);

        const int chunks = key_size_ / sizeof(int);
        int32_t* cast_key_ptr = (int32_t*)(key_ptr);
        for (size_t i = 0; i < chunks; ++i) {
            hash ^= cast_key_ptr[i];
            hash *= UINT64_C(1099511628211);
        }
        return hash;
    }

    uint32_t key_size_ = 4;
};

/* Lightweight wrapper to handle host input */
/* Key supports elementary types: int, long, etc. */
/* Value supports arbitrary types in theory. */
/* std::vector<bool> is specialized: it stores only one bit per element
 * We have to use uint8_t instead to read and write masks
 * https://en.wikipedia.org/w/index.php?title=Sequence_container_(C%2B%2B)&oldid=767869909#Specialization_for_bool
 */
namespace cuda {
template <typename Hash = hash, class MemMgr = open3d::MemoryManager>
class Hashmap {
public:
    Hashmap(uint32_t max_keys,
            uint32_t dsize_key,
            uint32_t dsize_value,
            uint32_t dsize_kvpair,
            // Preset hash table params to estimate bucket num
            uint32_t keys_per_bucket = 10,
            float expected_occupancy_per_bucket = 0.5,
            // CUDA device
            open3d::Device device = open3d::Device("CUDA:0"));
    ~Hashmap();

    /// Thrust interface for array-like input
    /// TODO: change to raw pointers and adapt to Tensor scheduling
    /// Insert keys and values.
    /// Return (CUDA) pointers to the inserted kv pairs.
    std::pair<thrust::device_vector<iterator_t>, thrust::device_vector<uint8_t>>
    Insert(uint8_t* input_keys, uint8_t* input_values, uint32_t input_key_size);

    /// Search keys.
    /// Return (CUDA) pointers to the found kv pairs; nullptr for not found.
    /// Also returns a mask array to indicate success.
    std::pair<thrust::device_vector<iterator_t>, thrust::device_vector<uint8_t>>
    Search(uint8_t* input_keys, uint32_t input_key_size);

    /// Remove key value pairs given keys.
    /// Return mask array to indicate success or not found.
    thrust::device_vector<uint8_t> Remove(uint8_t* input_keys,
                                          uint32_t input_key_size);

    /// Assistance functions for memory profiling.
    float ComputeLoadFactor();
    std::vector<int> CountElemsPerBucket();

private:
    // Rough estimation of total keys at max capacity.
    // TODO: change it adaptively in internal implementation
    uint32_t max_keys_;
    uint32_t num_buckets_;

    uint32_t dsize_key_;
    uint32_t dsize_value_;
    uint32_t dsize_kvpair_;

    // Buffer to store temporary results
    uint8_t* output_key_buffer_;
    uint8_t* output_value_buffer_;
    iterator_t* output_iterator_buffer_;
    uint8_t* output_mask_buffer_;

    std::shared_ptr<HashmapCUDA<Hash, MemMgr>> device_hashmap_;
    open3d::Device device_;
};

template <typename Hash, class MemMgr>
Hashmap<Hash, MemMgr>::Hashmap(
        uint32_t max_keys,
        uint32_t dsize_key,
        uint32_t dsize_value,
        uint32_t dsize_kvpair,
        uint32_t keys_per_bucket,
        float expected_occupancy_per_bucket,
        open3d::Device device /* = open3d::Device("CUDA:0") */)
    : max_keys_(max_keys),
      dsize_key_(dsize_key),
      dsize_value_(dsize_value),
      dsize_kvpair_(dsize_kvpair),
      device_(device),
      device_hashmap_(nullptr) {
    // Set bucket size
    uint32_t expected_keys_per_bucket =
            expected_occupancy_per_bucket * keys_per_bucket;
    num_buckets_ = (max_keys + expected_keys_per_bucket - 1) /
                   expected_keys_per_bucket;

    // Allocate memory
    output_key_buffer_ =
            (uint8_t*)MemMgr::Malloc(max_keys_ * dsize_key_, device_);
    output_value_buffer_ =
            (uint8_t*)MemMgr::Malloc(max_keys_ * dsize_value_, device_);
    output_mask_buffer_ =
            (uint8_t*)MemMgr::Malloc(max_keys_ * sizeof(uint8_t), device_);
    output_iterator_buffer_ = (iterator_t*)MemMgr::Malloc(
            max_keys_ * sizeof(iterator_t), device_);

    // Initialize internal allocator
    device_hashmap_ = std::make_shared<HashmapCUDA<Hash, MemMgr>>(
            num_buckets_, max_keys_, dsize_key_, dsize_value_, dsize_kvpair_,
            device_);
}

template <typename Hash, class MemMgr>
Hashmap<Hash, MemMgr>::~Hashmap() {
    MemMgr::Free(output_key_buffer_, device_);
    MemMgr::Free(output_value_buffer_, device_);
    MemMgr::Free(output_mask_buffer_, device_);
    MemMgr::Free(output_iterator_buffer_, device_);
}

template <typename Hash, class MemMgr>
std::pair<thrust::device_vector<iterator_t>, thrust::device_vector<uint8_t>>
Hashmap<Hash, MemMgr>::Insert(uint8_t* input_keys,
                              uint8_t* input_values,
                              uint32_t input_keys_size) {
    // TODO: rehash and increase max_keys_
    assert(input_keys_size <= max_keys_);

    device_hashmap_->Insert(input_keys, input_values, output_iterator_buffer_,
                            output_mask_buffer_, input_keys_size);

    thrust::device_vector<iterator_t> output_iterators(
            output_iterator_buffer_, output_iterator_buffer_ + input_keys_size);
    thrust::device_vector<uint8_t> output_masks(
            output_mask_buffer_, output_mask_buffer_ + input_keys_size);
    return std::make_pair(output_iterators, output_masks);
}

template <typename Hash, class MemMgr>
std::pair<thrust::device_vector<iterator_t>, thrust::device_vector<uint8_t>>
Hashmap<Hash, MemMgr>::Search(uint8_t* input_keys, uint32_t input_keys_size) {
    assert(input_keys_size <= max_keys_);

    OPEN3D_CUDA_CHECK(cudaMemset(output_mask_buffer_, 0,
                                 sizeof(uint8_t) * input_keys_size));

    device_hashmap_->Search(input_keys, output_iterator_buffer_,
                            output_mask_buffer_, input_keys_size);
    OPEN3D_CUDA_CHECK(cudaDeviceSynchronize());

    thrust::device_vector<iterator_t> output_iterators(
            output_iterator_buffer_, output_iterator_buffer_ + input_keys_size);
    thrust::device_vector<uint8_t> output_masks(
            output_mask_buffer_, output_mask_buffer_ + input_keys_size);
    return std::make_pair(output_iterators, output_masks);
}

template <typename Hash, class MemMgr>
thrust::device_vector<uint8_t> Hashmap<Hash, MemMgr>::Remove(
        uint8_t* input_keys, uint32_t input_keys_size) {
    OPEN3D_CUDA_CHECK(cudaMemset(output_mask_buffer_, 0,
                                 sizeof(uint8_t) * input_keys_size));

    device_hashmap_->Remove(input_keys, output_mask_buffer_, input_keys_size);

    thrust::device_vector<uint8_t> output_masks(
            output_mask_buffer_, output_mask_buffer_ + input_keys_size);
    return output_masks;
}

template <typename Hash, class MemMgr>
std::vector<int> Hashmap<Hash, MemMgr>::CountElemsPerBucket() {
    return device_hashmap_->CountElemsPerBucket();
}

template <typename Hash, class MemMgr>
float Hashmap<Hash, MemMgr>::ComputeLoadFactor() {
    return device_hashmap_->ComputeLoadFactor();
}
}  // namespace cuda
