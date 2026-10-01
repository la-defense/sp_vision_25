cmake_minimum_required(VERSION 3.16)

find_package(yaml-cpp REQUIRED)
add_subdirectory(serial)

set(IO_SOURCES
  usbcamera/usbcamera.cpp
  tcp_frame_camera.cpp
  camera.cpp
  cboard.cpp
  dm_imu/dm_imu.cpp
  gimbal/protocol.cpp
  gimbal/gimbal.cpp
)

if(SP_ENABLE_HIKROBOT)
  list(APPEND IO_SOURCES hikrobot/hikrobot.cpp)
endif()
if(SP_ENABLE_MINDVISION)
  list(APPEND IO_SOURCES mindvision/mindvision.cpp)
endif()

if(NOT SP_ENABLE_HIKROBOT AND NOT SP_ENABLE_MINDVISION)
  message(STATUS "Building without a hardware camera SDK; camera input is unavailable")
endif()

add_library(io STATIC ${IO_SOURCES})
target_include_directories(io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(io PUBLIC yaml-cpp serial Eigen3::Eigen)

if(SP_ENABLE_HIKROBOT)
  target_compile_definitions(io PUBLIC SP_ENABLE_HIKROBOT=1)
  target_include_directories(io PUBLIC hikrobot/include)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(HIKROBOT_LIB_DIR hikrobot/lib/amd64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(HIKROBOT_LIB_DIR hikrobot/lib/arm64)
  else()
    message(FATAL_ERROR "Unsupported HikRobot SDK architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  find_library(HIKROBOT_CAMERA_LIBRARY MvCameraControl
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/${HIKROBOT_LIB_DIR}" NO_DEFAULT_PATH)
  if(NOT HIKROBOT_CAMERA_LIBRARY)
    message(FATAL_ERROR "HikRobot SDK library libMvCameraControl.so was not found")
  endif()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
  target_include_directories(io PRIVATE ${LIBUSB_INCLUDE_DIRS})
  target_link_directories(io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${HIKROBOT_LIB_DIR}")
  target_link_libraries(io PUBLIC ${HIKROBOT_CAMERA_LIBRARY} ${LIBUSB_LIBRARIES})
  set_property(TARGET io APPEND PROPERTY BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/${HIKROBOT_LIB_DIR}")
  set_property(TARGET io APPEND PROPERTY INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/${HIKROBOT_LIB_DIR}")
endif()

if(SP_ENABLE_MINDVISION)
  target_compile_definitions(io PUBLIC SP_ENABLE_MINDVISION=1)
  target_include_directories(io PUBLIC mindvision/include)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(MINDVISION_LIB_DIR mindvision/lib/amd64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(MINDVISION_LIB_DIR mindvision/lib/arm64)
  else()
    message(FATAL_ERROR "Unsupported MindVision SDK architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  find_library(MINDVISION_CAMERA_LIBRARY MVSDK
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/${MINDVISION_LIB_DIR}" NO_DEFAULT_PATH)
  if(NOT MINDVISION_CAMERA_LIBRARY)
    message(FATAL_ERROR "MindVision SDK library libMVSDK.so was not found")
  endif()
  target_link_directories(io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${MINDVISION_LIB_DIR}")
  target_link_libraries(io PUBLIC ${MINDVISION_CAMERA_LIBRARY})
  set_property(TARGET io APPEND PROPERTY BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/${MINDVISION_LIB_DIR}")
  set_property(TARGET io APPEND PROPERTY INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/${MINDVISION_LIB_DIR}")
endif()

find_package(ament_cmake QUIET)
find_package(rclcpp QUIET)
find_package(std_msgs QUIET)
find_package(rosidl_typesupport_cpp QUIET)
find_package(sp_msgs QUIET)
if(ament_cmake_FOUND AND rclcpp_FOUND AND std_msgs_FOUND AND rosidl_typesupport_cpp_FOUND AND sp_msgs_FOUND)
  target_sources(io PRIVATE ros2/publish2nav.cpp ros2/ros2.cpp ros2/subscribe2nav.cpp)
  ament_target_dependencies(io rclcpp std_msgs sp_msgs)
endif()
