# おまじない
cmake_minimum_required(VERSION 3.8)
project(feetech_driver)

# 警告を多めに出す
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# amentのCMakeの関数を読み込む
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_ros REQUIRED)

# feetech 公式SDKを、静的ライブラリにする
add_library(scservo STATIC
  vendor/scservo/src/HLSCL.cpp
  vendor/scservo/src/SCS.cpp
  vendor/scservo/src/SCSerial.cpp
)

# SDKのヘッダを探す
target_include_directories(scservo
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/scservo/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/scservo/include/scservo
)

# SDKの警告を消す
target_compile_options(scservo PRIVATE -w)

# -fPICを付ける
set_target_properties(scservo PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 自作のライブラリ（PRIVATE）
add_library(feetech_driver SHARED
  src/joint_config.cpp
  src/servo_bus.cpp
)

# おまじない
add_library(feetech_driver::feetech_driver ALIAS feetech_driver)

# C++ 17を要求
target_compile_features(feetech_driver PUBLIC cxx_std_17)

# 自作のヘッダを探す
target_include_directories(feetech_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)

# SDKをリンクする（PRIVATE）
target_link_libraries(feetech_driver PRIVATE scservo)

# 公開ヘッダを install/ にコピーする
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

# .so を install/ にコピー
install(
  TARGETS feetech_driver
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# 手で使う道具（tools）
add_executable(feetech_scan tools/feetech_scan.cpp)
target_link_libraries(feetech_scan feetech_driver)
add_executable(feetech_jog tools/feetech_jog.cpp)
target_link_libraries(feetech_jog feetech_driver)
install(TARGETS feetech_scan feetech_jog DESTINATION lib/${PROJECT_NAME})


# colcon test で走るlint と gtest
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  file(GLOB_RECURSE _vendor_files vendor/*)
  set(AMENT_LINT_AUTO_FILE_EXCLUDE ${_vendor_files})
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_joint_config test/test_joint_config.cpp)
  target_link_libraries(test_joint_config feetech_driver)
endif()


# 一覧を使う側に渡す
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)

# おまじない
ament_package()
