cmake_minimum_required(VERSION 3.24)
project(HelloCyCode LANGUAGES CXX)
add_executable(hello main.cpp)
target_compile_features(hello PRIVATE cxx_std_20)
