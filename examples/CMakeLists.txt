# CPP examples

if (BUILD_CPP_EXAMPLES)
    add_subdirectory(Cpp)
endif ()

message(WARNING "BUILDING Examples")
# Cuda module -- Enable it here
#if (BUILD_CUDA_EXAMPLES)
message(WARNING "BUILDING CUDA")
add_subdirectory(Cuda)
#endif()

# Python examples
if (BUILD_PYTHON_MODULE)
    add_subdirectory(Python)
endif ()
