//
// Created by wei on 10/4/18.
//

#pragma once

#include <Cuda/Common/LinearAlgebraCuda.h>
#include <cstdlib>

namespace open3d {

namespace cuda {

/** Approximated by JtJ in optimization **/
template<size_t N>
class HessianCuda {
    /**
     * 0  1  2  3  4  5
     *    6  7  8  9  10
     *       11 12 13 14
     *          15 16 17
     *             18 19
     *                20
     **/
private:
    float h_[(N + 1) * N / 2];

public:
    inline __HOSTDEVICE__ float &operator()(size_t i) {
#ifdef CUDA_DEBUG_ENABLE_ASSERTION
        assert(i < (N + 1) * N / 2);
#endif
        return h_[i];
    }

    inline __HOSTDEVICE__ const float &operator()(size_t i) const {
#ifdef CUDA_DEBUG_ENABLE_ASSERTION
        assert(i < (N + 1) * N / 2);
#endif
        return h_[i];
    }
};

namespace {
/** Joint terms **/
__HOSTDEVICE__ void ComputeJtJAndJtr(
    const Vector6f &jacobian_I, const Vector6f &jacobian_G,
    const float &residual_I, const float &residual_G,
    HessianCuda<6> &JtJ, Vector6f &Jtr) {

    int cnt = 0;
#ifdef __CUDACC__
#pragma unroll 1
#endif
    for (int i = 0; i < 6; ++i) {
#ifdef __CUDACC__
#pragma unroll 1
#endif
        for (int j = i; j < 6; ++j) {
            JtJ(cnt++) = jacobian_I(i) * jacobian_I(j)
                + jacobian_G(i) * jacobian_G(j);
        }
        Jtr(i) = jacobian_I(i) * residual_I + jacobian_G(i) * residual_G;
    }
}

/** Single term **/
__HOSTDEVICE__ void ComputeJtJAndJtr(
    const Vector6f &jacobian_I, const float &residual_I,
    HessianCuda<6> &JtJ, Vector6f &Jtr) {

    int cnt = 0;
#ifdef __CUDACC__
#pragma unroll 1
#endif
    for (int i = 0; i < 6; ++i) {
#ifdef __CUDACC__
#pragma unroll 1
#endif
        for (int j = i; j < 6; ++j) {
            JtJ(cnt++) = jacobian_I(i) * jacobian_I(j);
        }
        Jtr(i) = jacobian_I(i) * residual_I;
    }
}
} // unnamed namespace

} // cuda
} // open3d