# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

# Declares and names the project.

project("AndFix")

set(CMAKE_CXX_FLAGS "-std=c++11")

set(SRC_LIST
    andfix.cpp
    art/art_method_replace.cpp
    art/art_method_replace_4_4.cpp
    art/art_method_replace_5_0.cpp
    art/art_method_replace_5_1.cpp
    art/art_method_replace_6_0.cpp
    art/art_method_replace_7_0.cpp
    dalvik/dalvik_method_replace.cpp
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../libs/${ANDROID_ABI})

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library(
    # Sets the name of the library.
    andfix

    # Sets the library as a shared library.
    SHARED

    # Provides a relative path to your source file(s).
    ${SRC_LIST}
)

include_directories(
    art/art.h
    art/art_4_4.h
    art/art_5_0.h
    art/art_5_1.h
    art/art_6_0.h
    art/art_7_0.h
    dalvik/dalvik.h
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library(
    # Sets the name of the path variable.
    log-lib

    # Specifies the name of the NDK library that
    # you want CMake to locate.
    log
)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(
    # Specifies the target library.
    andfix

    # Links the target library to the log library
    # included in the NDK.
    ${log-lib}
)