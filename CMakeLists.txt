cmake_minimum_required(VERSION 3.15)
project(IRViewer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OptiTrack Camera SDK paths
set(CAMERA_SDK_PATH "C:/Program Files (x86)/OptiTrack/CameraSDK")
set(CAMERA_SDK_INCLUDE_DIR "${CAMERA_SDK_PATH}/include")
set(CAMERA_SDK_LIB_DIR "${CAMERA_SDK_PATH}/lib")
set(CAMERA_SDK_BIN_DIR "${CAMERA_SDK_PATH}/bin")

# OpenCV paths
set(OPENCV_PATH "C:/opencv/opencv/build")
set(OPENCV_INCLUDE_DIR "${OPENCV_PATH}/include")
set(OPENCV_LIB_DIR "${OPENCV_PATH}/x64/vc15/lib")
set(OPENCV_BIN_DIR "${OPENCV_PATH}/x64/vc15/bin")

# Add include directories
include_directories(
    ${CAMERA_SDK_INCLUDE_DIR}
    ${OPENCV_INCLUDE_DIR}
)

# Add library directories
link_directories(
    ${CAMERA_SDK_LIB_DIR}
    ${OPENCV_LIB_DIR}
)

# Create executable
add_executable(IRViewer main.cpp)

# Link libraries
# Use static library for Camera SDK
file(GLOB OPENCV_LIBS "${OPENCV_LIB_DIR}/opencv_world*.lib")
target_link_libraries(IRViewer
    CameraLibrary2019x64S.lib
    ${OPENCV_LIBS}
)

# Copy Camera SDK DLL to output directory after build
add_custom_command(TARGET IRViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CAMERA_SDK_LIB_DIR}/CameraLibrary2019x64S.dll"
        $<TARGET_FILE_DIR:IRViewer>
    COMMENT "Copying Camera SDK DLL"
)

# Copy OpenCV DLL if it exists
file(GLOB OPENCV_DLLS "${OPENCV_BIN_DIR}/opencv_world*.dll")
if(OPENCV_DLLS)
    add_custom_command(TARGET IRViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENCV_DLLS}
            $<TARGET_FILE_DIR:IRViewer>
        COMMENT "Copying OpenCV DLLs to output directory"
    )
endif()

# Set output directories
set_target_properties(IRViewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

# Print configuration info
message(STATUS "Camera SDK: ${CAMERA_SDK_PATH}")
message(STATUS "OpenCV Path: ${OPENCV_PATH}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
