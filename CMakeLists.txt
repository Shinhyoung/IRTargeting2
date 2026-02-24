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
    Ws2_32.lib   # Windows Sockets 2 (UDP 통신)
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

# ===== UDP Receiver (테스트용 수신 프로그램) =====
add_executable(UDPReceiver udp_receiver.cpp)
target_link_libraries(UDPReceiver
    ${OPENCV_LIBS}
    Ws2_32.lib
)
set_target_properties(UDPReceiver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Debug"
)
# OpenCV DLL 복사 (Release 디렉토리에 이미 있으므로 중복 시 덮어쓰기)
if(OPENCV_DLLS)
    add_custom_command(TARGET UDPReceiver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENCV_DLLS}
            $<TARGET_FILE_DIR:UDPReceiver>
        COMMENT "Copying OpenCV DLLs for UDPReceiver"
    )
endif()

# Print configuration info
message(STATUS "Camera SDK: ${CAMERA_SDK_PATH}")
message(STATUS "OpenCV Path: ${OPENCV_PATH}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
