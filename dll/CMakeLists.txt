
add_library(dwm_qod SHARED dll.cpp)
target_include_directories(dwm_qod PRIVATE minhook)
target_link_libraries(dwm_qod PRIVATE minhook)