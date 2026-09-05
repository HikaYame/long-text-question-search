cmake_minimum_required(VERSION 3.18)
project(long_text_question_search LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(search_core search_engine.cpp)
target_include_directories(search_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
