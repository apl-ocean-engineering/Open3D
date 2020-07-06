
# Modified to add cuda files
macro(add_cuda_source_group module_name)
    file(GLOB MODULE_HEADER_FILES "${module_name}/*.cuh")
    source_group("Header Files\\${module_name}" FILES ${MODULE_HEADER_FILES})
    file(GLOB MODULE_SOURCE_FILES "${module_name}/*.cu")
    source_group("Source Files\\${module_name}" FILES ${MODULE_SOURCE_FILES})
endmacro()

#enable_language(CUDA)
find_package(CUDA REQUIRED)

#list(APPEND CUDA_NVCC_FLAGS "-std=c++11 -rdc=true")
set(CUDA_PROPAGATE_HOST_FLAGS OFF)
#add_definitions(-std=c++11)

set(CUDA_SEPARABLE_COMPILATION ON)

# GLOB all required files in this subdirectory
file(GLOB_RECURSE CUDA_ALL_HEADER_FILES "*.h")
file(GLOB_RECURSE CUDA_ALL_SOURCE_FILES "*.cpp")
file(GLOB_RECURSE CUDA_ALL_CUDA_HEADER_FILES "*.cuh")
file(GLOB_RECURSE CUDA_ALL_CUDA_SOURCE_FILES "*.cu")

# create subgroups
add_source_group(Common)
add_source_group(Camera)
add_source_group(Container)
add_source_group(Geometry)
add_source_group(Integration)
add_source_group(Registration)
add_source_group(IO)
add_source_group(Odometry)
add_source_group(Visualization)

# and also for cuda
add_cuda_source_group(Common)
add_cuda_source_group(Camera)
add_cuda_source_group(Container)
add_cuda_source_group(Geometry)
add_cuda_source_group(Integration)
add_cuda_source_group(Registration)
add_cuda_source_group(IO)
add_cuda_source_group(Odometry)
add_cuda_source_group(Visualization)

# create (should be static) library
set(OPEN3D_CUDA_MODULE "Open3DCuda" CACHE STRING "CUDA component library")

add_library(${OPEN3D_CUDA_MODULE}
        ${CUDA_ALL_HEADER_FILES}
        ${CUDA_ALL_SOURCE_FILES}
        ${CUDA_ALL_CUDA_HEADER_FILES}
        ${CUDA_ALL_CUDA_SOURCE_FILES}
        $<TARGET_OBJECTS:Camera>
        $<TARGET_OBJECTS:ColorMap>
        $<TARGET_OBJECTS:Geometry>
        $<TARGET_OBJECTS:Integration>
        $<TARGET_OBJECTS:Odometry>
        $<TARGET_OBJECTS:Registration>
        $<TARGET_OBJECTS:Utility>
        $<TARGET_OBJECTS:IO>
        $<TARGET_OBJECTS:Visualization>)
# Suppressing this since there are too many warnings
# open3d_show_and_abort_on_warning(${OPEN3D_CUDA_MODULE})
open3d_set_global_properties(${OPEN3D_CUDA_MODULE})
open3d_link_3rdparty_libraries(${OPEN3D_CUDA_MODULE})

add_library(${PROJECT_NAME}::${OPEN3D_CUDA_MODULE} ALIAS ${OPEN3D_CUDA_MODULE})

target_link_libraries(${OPEN3D_CUDA_MODULE} PUBLIC ${CUDA_LIBRARIES} -lcublas -lcurand)

# Installation
install(TARGETS ${OPEN3D_CUDA_MODULE} EXPORT ${OPEN3D_CUDA_MODULE}Targets
        RUNTIME DESTINATION ${Open3D_INSTALL_BIN_DIR}
        LIBRARY DESTINATION ${Open3D_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${Open3D_INSTALL_LIB_DIR}
)

#install headers
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DESTINATION "${Open3D_INSTALL_INCLUDE_DIR}"
        PATTERN "*.cpp" EXCLUDE
        PATTERN "*.cuh" EXCLUDE
        PATTERN "*.cu" EXCLUDE
        PATTERN "*.txt" EXCLUDE)

# uninstall target
if (NOT TARGET uninstall)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
                   IMMEDIATE @ONLY)

    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P
                      ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

