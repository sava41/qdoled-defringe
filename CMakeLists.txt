cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 20)
include(FetchContent)

project(QdOledDefringe)

FetchContent_Declare(
  MinHook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG master
)
FetchContent_MakeAvailable(MinHook)

add_subdirectory(dll)