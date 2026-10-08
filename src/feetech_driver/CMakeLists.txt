cmake_minimum_required(VERSION 3.8)
project(feetech_driver)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_ros REQUIRED)

# 同梱した Feetech 公式 SDK（書き換えない）。HLSCL → SCSerial → SCS の 3 つだけ使う
add_library(scservo STATIC
  vendor/scservo/src/HLSCL.cpp
  vendor/scservo/src/SCS.cpp
  vendor/scservo/src/SCSerial.cpp
)
target_include_directories(scservo
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/scservo/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/scservo/include/scservo
)
target_compile_options(scservo PRIVATE -w)
set_target_properties(scservo PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 自作のライブラリ。SDK は中でだけ使う（PRIVATE）
add_library(feetech_driver SHARED
  src/joint_config.cpp
)
add_library(feetech_driver::feetech_driver ALIAS feetech_driver)
target_compile_features(feetech_driver PUBLIC cxx_std_17)
target_include_directories(feetech_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_link_libraries(feetech_driver PRIVATE scservo)

install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)
install(
  TARGETS feetech_driver
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)

ament_package()
