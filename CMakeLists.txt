cmake_minimum_required(VERSION 3.19)
project(QdOledDefringe LANGUAGES CSharp CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8")
set(CMAKE_CSharp_FLAGS "/platform:x64")
set(CMAKE_CSharp_FLAGS "/langversion:8")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/binaries/")

set(CMAKE_INSTALL_BINARY_DIR    "bin/")
set(CMAKE_INSTALL_LIBRARY_DIR   "bin/")
set(CMAKE_INSTALL_INCLUDE_DIR   "include/")
set(CMAKE_INSTALL_EXPORT_DIR    "cmake/")

add_subdirectory(source/dll)
add_subdirectory(source/gui)