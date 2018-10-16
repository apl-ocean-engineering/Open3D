//
// Created by wei on 10/9/18.
//

#pragma once

#include "IntegrationClasses.h"
#include <Cuda/Geometry/VectorCuda.h>
#include <Cuda/Geometry/ImageCuda.h>
#include <Cuda/Geometry/PinholeCameraCuda.h>
#include <Cuda/Geometry/TransformCuda.h>
#include <Cuda/Geometry/TriangleMeshCuda.h>

#include <cstdlib>
#include <memory>

namespace open3d {

template<size_t N>
class __ALIGN__(16) UniformTSDFVolumeCudaServer {
private:
    /** [N * N * N]; **/
    float *tsdf_;
    uchar *weight_;
    Vector3b *color_;

    /* Embedded vertices */
    int *table_index_;
    Vector3i *vertex_indices_;

    TriangleMeshCudaServer mesh_;

public:
    /** According to UniformTSDFVolume.cpp,
     * Voxel xyz is at Vector3f(0.5) + [x, y, z]^T * voxel_length_;
     */
    float voxel_length_;
    float inv_voxel_length_;
    float sdf_trunc_;
    TransformCuda transform_volume_to_world_;
    TransformCuda transform_world_to_volume_;

public:
    inline __DEVICE__ TriangleMeshCudaServer& mesh() {
        return mesh_;
    }

    /** Conversions **/
    inline __DEVICE__ Vector3f world_to_voxel(float x, float y, float z);
    inline __DEVICE__ Vector3f world_to_voxel(const Vector3f &X);

    inline __DEVICE__ Vector3f voxel_to_world(float x, float y, float z);
    inline __DEVICE__ Vector3f voxel_to_world(const Vector3f &X);

    inline __DEVICE__ Vector3f voxel_to_volume(const Vector3f &X);
    inline __DEVICE__ Vector3f voxel_to_volume(float x, float y, float z);

    inline __DEVICE__ Vector3f volume_to_voxel(const Vector3f &X);
    inline __DEVICE__ Vector3f volume_to_voxel(float x, float y, float z);

    inline __DEVICE__ Vector3i Vectorize(size_t index);
    inline __DEVICE__ int IndexOf(int x, int y, int z);
    inline __DEVICE__ int IndexOf(const Vector3i &X);

    /** Below all the coordinates (int and float) are in Volume coordinate
     * system **/

    /** Direct index accessing
     * - for efficiency ignore index checking in these functions
     * - check them outside **/
    inline __DEVICE__ bool InVolume(int x, int y, int z);
    inline __DEVICE__ bool InVolume(const Vector3i &X);

    inline __DEVICE__ float &tsdf(int x, int y, int z);
    inline __DEVICE__ float &tsdf(const Vector3i &X);
    inline __DEVICE__ uchar &weight(int x, int y, int z);
    inline __DEVICE__ uchar &weight(const Vector3i &X);
    inline __DEVICE__ Vector3b &color(int x, int y, int z);
    inline __DEVICE__ Vector3b &color(const Vector3i &X);

    /** Voxel level trivial gradient **/
    inline __DEVICE__ Vector3f gradient(int x, int y, int z);
    inline __DEVICE__ Vector3f gradient(const Vector3i &X);


    inline __DEVICE__ Vector3i &vertex_indices(int x, int y, int z);
    inline __DEVICE__ Vector3i &vertex_indices(const Vector3i &X);
    inline __DEVICE__ int &table_index(int x, int y, int z);
    inline __DEVICE__ int &table_index(const Vector3i &X);

    /** Value interpolating **/
    inline __DEVICE__ bool InVolumef(float x, float y, float z);
    inline __DEVICE__ bool InVolumef(const Vector3f &X);

    inline __DEVICE__ float TSDFAt(float x, float y, float z);
    inline __DEVICE__ float TSDFAt(const Vector3f &X);
    inline __DEVICE__ uchar WeightAt(float x, float y, float z);
    inline __DEVICE__ uchar WeightAt(const Vector3f &X);
    inline __DEVICE__ Vector3b ColorAt(float x, float y, float z);
    inline __DEVICE__ Vector3b ColorAt(const Vector3f &X);

    inline __DEVICE__ Vector3f GradientAt(float x, float y, float z);
    inline __DEVICE__ Vector3f GradientAt(const Vector3f &X);

public:
    friend class UniformTSDFVolumeCuda<N>;
};

template<size_t N>
class UniformTSDFVolumeCuda {
private:
    std::shared_ptr<UniformTSDFVolumeCudaServer<N>> server_ = nullptr;
    TriangleMeshCuda mesh_;

public:
    float voxel_length_;
    float sdf_trunc_;
    TransformCuda transform_volume_to_world_;

public:
    UniformTSDFVolumeCuda();
    UniformTSDFVolumeCuda(float voxel_length, float sdf_trunc,
                          TransformCuda &volume_to_world = TransformCuda::Identity());
    UniformTSDFVolumeCuda(const UniformTSDFVolumeCuda<N> &other);
    UniformTSDFVolumeCuda<N> &operator=(const UniformTSDFVolumeCuda<N> &other);
    ~UniformTSDFVolumeCuda();

    void Create();
    void Release();
    void UpdateServer();

    void Reset();

    void UploadVolume(std::vector<float> &tsdf, std::vector<uchar> &weight,
                      std::vector<Vector3b> &color);
    std::tuple<std::vector<float>, std::vector<uchar>, std::vector<Vector3b>>
    DownloadVolume();

    int Integrate(ImageCuda<Vector1f> &depth,
                  MonoPinholeCameraCuda &camera,
                  TransformCuda &transform_camera_to_world);
    int MarchingCubes();

    int RayCasting(ImageCuda<Vector3f> &image,
                   MonoPinholeCameraCuda &camera,
                   TransformCuda &transform_camera_to_world);

public:
    TriangleMeshCuda &mesh() {
        return mesh_;
    }
    const TriangleMeshCuda &mesh() const {
        return mesh_;
    }
    std::shared_ptr<UniformTSDFVolumeCudaServer<N>> &server() {
        return server_;
    }
    const std::shared_ptr<UniformTSDFVolumeCudaServer<N>> &server() const {
        return server_;
    }
};

template<size_t N>
__GLOBAL__
void IntegrateKernel(UniformTSDFVolumeCudaServer<N> server,
                     ImageCudaServer<Vector1f> depth,
                     MonoPinholeCameraCuda camera,
                     TransformCuda transform_camera_to_world);

template<size_t N>
__GLOBAL__
void RayCastingKernel(UniformTSDFVolumeCudaServer<N> server,
                      ImageCudaServer<Vector3f> image,
                      MonoPinholeCameraCuda camera,
                      TransformCuda transform_camera_to_world);

template<size_t N>
__GLOBAL__
void MarchingCubesVertexAllocationKernel(UniformTSDFVolumeCudaServer<N> server);

template<size_t N>
__GLOBAL__
void MarchingCubesVertexExtractionKernel(UniformTSDFVolumeCudaServer<N> server);

template<size_t N>
__GLOBAL__
void MarchingCubesTriangleExtractionKernel(UniformTSDFVolumeCudaServer<N> server);
}