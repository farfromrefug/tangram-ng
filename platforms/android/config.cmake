add_definitions(-DTANGRAM_ANDROID)

if ($ENV{CIRCLECI})
  # Force Ninja on CircleCI to use a specific number of concurrent jobs.
  # Otherwise it guesses concurrency based on the physical CPU (which has a lot of cores!) and runs out of memory.
  message(STATUS "Limiting concurrent jobs due to CI environment.")
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS compile_pool=4 link_pool=2)
  set(CMAKE_JOB_POOL_COMPILE compile_pool)
  set(CMAKE_JOB_POOL_LINK link_pool)
endif()

add_library(tangram SHARED
  platforms/common/platform_gl.cpp
  platforms/common/user_fns.cpp
  platforms/android/tangram/src/main/cpp/JniHelpers.cpp
  platforms/android/tangram/src/main/cpp/JniOnLoad.cpp
  platforms/android/tangram/src/main/cpp/AndroidPlatform.cpp
  platforms/android/tangram/src/main/cpp/AndroidMap.cpp
  platforms/android/tangram/src/main/cpp/NativeMap.cpp
)

target_include_directories(tangram PRIVATE
  core/deps/stb
  core/deps/glm
  core/deps/isect2d/include
)

target_compile_definitions(tangram PRIVATE TANGRAM_ANDROID_MAIN=1)
target_compile_definitions(tangram PRIVATE GLM_FORCE_CTOR_INIT)

if(TANGRAM_MBTILES_DATASOURCE)
  target_sources(tangram PRIVATE platforms/android/tangram/src/main/cpp/sqlite3ndk.cpp)
  target_include_directories(tangram PRIVATE core/deps/sqlite3) # sqlite3ndk.cpp needs sqlite3.h
  target_compile_definitions(tangram PRIVATE TANGRAM_MBTILES_DATASOURCE=1)
endif()

target_link_libraries(tangram
  PRIVATE
  tangram-core
  # android libraries
  android
  atomic
  GLESv2
  log
  z
  jnigraphics
)
