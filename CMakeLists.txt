cmake_minimum_required(VERSION 3.17)
project(MST_Sortledton)

# 1. 基础环境设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 查找依赖
find_package(TBB REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)
find_library(TCMALLOC_LIB tcmalloc)

# =========================================================
# 辅助宏：递归查找包含头文件的所有目录
# =========================================================
macro(get_all_include_directories return_list current_dir)
    file(GLOB_RECURSE new_list LIST_DIRECTORIES true "${current_dir}/*.h" "${current_dir}/*.hpp")
    set(dir_list "")
    foreach(file_path ${new_list})
        get_filename_component(dir_path ${file_path} PATH)
        list(APPEND dir_list ${dir_path})
    endforeach()
    list(REMOVE_DUPLICATES dir_list)
    set(${return_list} ${dir_list})
endmacro()
# 自动收集所有包含 .h/.hpp 的目录
get_all_include_directories(ALL_INC_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# 打印一下看看找到了哪些路径 (调试用，不需要可以注释掉)
# message(STATUS "Found Include Directories: ${ALL_INC_DIRS}")

# 3. 收集源文件 (排除包含 main 函数的文件)
file(GLOB_RECURSE ALGORITHM_SRC "algorithm/*.cpp")
file(GLOB_RECURSE DATA_STRUCT_SRC "data-structure/*.cpp")
file(GLOB_RECURSE UTILS_SRC "utils/*.cpp")
file(GLOB_RECURSE MEMORY_SRC "MemoryPool/*.cpp")
file(GLOB_RECURSE HAL_MEMORY_SRC "HALMemoryPool/*.cpp")
file(GLOB_RECURSE THIRD_PARTY_SRC "third-party/*.cpp")
file(GLOB TRANSACTION_SRC "data-structure/Transaction/*.cpp")

set(AVB_SOURCES 
    ${ALGORITHM_SRC}
    ${DATA_STRUCT_SRC}
    ${UTILS_SRC}
    ${MEMORY_SRC}
    ${HAL_MEMORY_SRC}
    ${THIRD_PARTY_SRC}
    ${TRANSACTION_SRC}
)

# =========================================================
# 目标 A: 生成静态库 AVB_Core
# =========================================================
add_library(AVB_Core STATIC ${AVB_SOURCES})

# 使用我们自动收集到的所有路径
target_include_directories(AVB_Core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR} # 根目录通常也需要
    ${ALL_INC_DIRS}
)

# 链接依赖
target_link_libraries(AVB_Core PUBLIC 
    TBB::tbb 
    OpenMP::OpenMP_CXX 
    Threads::Threads
)

if(TCMALLOC_LIB)
    target_link_libraries(AVB_Core PUBLIC ${TCMALLOC_LIB})
endif()

# 编译选项
target_compile_options(AVB_Core PRIVATE 
     -Wno-deprecated-declarations -Wno-deprecated -g -fopenmp -O3 -std=c++17 -lpthread -ltbb -ltcmalloc -DJEMALLOC_NO_DEMANGLE
)

# =========================================================
# 目标 B: 保持你原来的可执行文件 (test_version_block)
# =========================================================
add_executable(test_version_block test/test_version_block.cpp)
target_link_libraries(test_version_block PRIVATE AVB_Core)
target_compile_options(test_version_block PRIVATE
    -Wno-deprecated-declarations -Wno-deprecated -g -fopenmp -O3 -std=c++17 -lpthread -ltbb -ltcmalloc -DJEMALLOC_NO_DEMANGLE
)