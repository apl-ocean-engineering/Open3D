# Integration
UNIT_TEST_CUDA(TestScalableTSDFVolumeCuda ${CMAKE_PROJECT_NAME} ${OPEN3D_CUDA_MODULE}
        ${GTEST_LIBRARIES} pthread)
UNIT_TEST_CUDA(TestUniformTSDFVolumeCuda ${CMAKE_PROJECT_NAME} ${OPEN3D_CUDA_MODULE}
        ${GTEST_LIBRARIES} pthread)