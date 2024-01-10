cmake_minimum_required(VERSION 3.0)  
project(sylar)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include (cmake/utils.cmake)    
set(CMAKE_VERBOSE_MAKEFILE ON)   
# 设置编译C++代码的编译器标志。这些标志将传递给C++编译器。  
# '-rdynamic'允许在运行时动态链接库。  
# '-Wall'和'-Wno-deprecated'以及'-Werror'和'-Wno-unused-function'是警告标志，它们分别启用或禁用特定的编译器警告，并将任何警告标记为错误。  
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")  
  
include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)


# 指定源代码文件，这些文件将用于构建sylar库。这些文件位于'sylar'目录下。  
set(LIB_SRC 
    sylar/config.cpp
    sylar/fiber.cpp
    sylar/fd_manager.cpp
    sylar/hook.cpp
    sylar/log.cpp
    sylar/iomanager.cpp
    sylar/mutex.cpp
    sylar/scheduler.cpp
    sylar/timer.cpp
    sylar/thread.cpp
    sylar/util.cpp
    )  

  
# 添加一个名为'sylar'的库，该库由前面指定的源代码文件编译而成。SHARED关键字表示这是一个共享库。  
add_library(sylar SHARED ${LIB_SRC}) 
force_redefine_file_macro_for_sources(sylar) #__FILE__   
# add_library(sylar_static STATIC ${LIB_SRC})  
# SET_TARGET_PROPERTIES (sylar_static PROPERTIES OUTPUT_NAME "sylar")  
  
find_library(YAMLCPP yaml-cpp)
find_library(PTHREAD pthread)


# 添加一个名为'test'的可执行文件，该可执行文件由名为'tests/test.cc'的源文件编译而成。 
# 指定'test'可执行文件依赖于'sylar'库。这意味着在构建'test'之前，'sylar'库必须先被构建。   
# 这行代码告诉CMake在链接'test'可执行文件时，需要链接到'sylar'库。这样，'test'可执行文件就可以使用'sylar'库中的功能了。
set(LIBS
    sylar
    dl
    pthread
    yaml-cpp
    )

message("***", ${LIBS})



add_executable(test tests/test.cpp)  
add_dependencies(test sylar)  
force_redefine_file_macro_for_sources(test)
target_link_libraries(test  ${LIBS})  

add_executable(test_config tests/test_config.cpp)
add_dependencies(test_config  sylar)
force_redefine_file_macro_for_sources(test_config)
target_link_libraries(test_config ${LIBS})

add_executable(test_thread tests/test_thread.cpp)
add_dependencies(test_thread  sylar)
force_redefine_file_macro_for_sources(test_thread)
target_link_libraries(test_thread ${LIBS})

add_executable(test_util tests/test_util.cpp)
add_dependencies(test_util  sylar)
force_redefine_file_macro_for_sources(test_util)
target_link_libraries(test_util ${LIBS})

add_executable(test_fiber tests/test_fiber.cpp)
add_dependencies(test_fiber  sylar)
force_redefine_file_macro_for_sources(test_fiber)
target_link_libraries(test_fiber ${LIBS})

add_executable(test_scheduler tests/test_scheduler.cpp)
add_dependencies(test_scheduler  sylar)
force_redefine_file_macro_for_sources(test_scheduler)
target_link_libraries(test_scheduler ${LIBS})

add_executable(test_iomanager tests/test_iomanager.cpp)
add_dependencies(test_iomanager  sylar)
force_redefine_file_macro_for_sources(test_iomanager)
target_link_libraries(test_iomanager ${LIBS})

add_executable(test_hook tests/test_hook.cpp)
add_dependencies(test_hook  sylar)
force_redefine_file_macro_for_sources(test_hook)
target_link_libraries(test_hook ${LIBS})
  
# 设置可执行文件的输出路径为源码目录下的'bin'目录。  
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)  
  
# 设置库文件的输出路径为源码目录下的'lib'目录。  
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)