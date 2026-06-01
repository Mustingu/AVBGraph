cmake_minimum_required (VERSION 3.17)
project (test1)

# 查找TBB包
find_package(TBB REQUIRED)
# find_package(tcmalloc REQUIRED)

# if(tcmalloc_FOUND)
#   massage("yes")
# else()
#   message("no")
# endif()
# 如果TBB被找到，它会设置TBB_INCLUDE_DIRS和TBB_LIBRARIES变量
if(TBB_FOUND)
  include_directories(${TBB_INCLUDE_DIRS})
  message(STATUS "TBB found: ${TBB_INCLUDE_DIRS}")
else()
  message(FATAL_ERROR "TBB not found.")
endif()

add_definitions(-DTBB_PREVIEW_CONCURRENT_ORDERED_CONTAINERS=1)

set(/home/masitan/MST_Sortledton ${CMAKE_CURRENT_SOURCE_DIR})
include_directories("./..")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../algorithm")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../data-structure")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../data-structure/EdgeBlock")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../data-structure/Interface")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../data-structure/Exception")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../data-structure/VersionBlock")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../data-structure/VersionBlock/VertexVersionBlock")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../MemoryPool")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../MemoryPool/FixSizeAllocator")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../MemoryPool/FreeListQueue")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../HALMemoryPool")

# message("${CMAKE_CURRENT_SOURCE_DIR}/../third-party")
# message(STATUS "CMAKE_INCLUDE_DIRECTORIES: ${CMAKE_INCLUDE_DIRECTORIES}")
# file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../MemoryPool/*.h")
# message(STATUS "Header files in MemoryPool: ${HEADER_FILES}")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-deprecated -g -fopenmp -O3 -std=c++17 -lpthread -ltbb -ltcmalloc -DJEMALLOC_NO_DEMANGLE")



# find_library(TCMALLOC_LIBRARY NAMES tcmalloc_minimal PATHS /usr/lib/x86_64-linux-gnu)
# target_link_libraries(test1 ${TCMALLOC_LIBRARY})






include_directories("../utils")
file(GLOB_RECURSE ALGORITHM "../algorithm/*.cpp")
file(GLOB_RECURSE DATA_STRUCT "../data-structure/*.cpp")
file(GLOB TRANSACTIONSOURCE "../data-structure/Transaction/*.cpp")
file(GLOB SLTCOMPONENTS "../data-structure/Sortledton_component/*.cpp")
file(GLOB SLTCOMPONENTS "../third-party/*.cpp")
file(GLOB_RECURSE MEMORYPOOL "../MemoryPool/*.cpp")
file(GLOB_RECURSE HALMEMORYPOOL "../HALMemoryPool/*.cpp")
file(GLOB_RECURSE UTILS "../utils/*.cpp")
# file(GLOB MEMORYPOOL "./../MemoryPool/FreeListQueue/*.cpp")
# file(GLOB MEMORYPOOL "./../MemoryPool/FixSizeAllocator/*.cpp")
# file(GLOB MEMORYPOOL "./../MemoryPool/FreeListQueue/*.cpp")
file(GLOB ALLSOURCE ${TRANSACTIONSOURCE} ${SLTCOMPONENTS} ${DATA_STRUCT} ${HALMEMORYPOOL} ${UTILS} ${ALGORITHM})

message(STATUS "CPP_SOURCES: ${ALLSOURCE}")


# add_executable(test1 test1.cpp ${MEMORYPOOL})
# target_link_libraries(test1 pthread)


# find_library(JEMALLOC_LIBRARY NAMES jemalloc PATHS /usr/lib/x86_64-linux-gnu)
# target_link_libraries(test1 ${JEMALLOC_LIBRARY})
# target_link_libraries(test1 ${TBB_IMPORTED_TARGETS} ${TBB_tbb_lib})


add_executable(test_version_block test_version_block.cpp ${ALLSOURCE})


target_link_libraries(test_version_block pthread)
target_link_libraries(test_version_block ${TCMALLOC_LIBRARY})
target_link_libraries(test_version_block ${TBB_IMPORTED_TARGETS} ${TBB_tbb_lib})


# add_executable(test_boost test_boost.cpp)
# target_link_libraries(test_boost pthread)
