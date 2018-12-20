cmake_minimum_required(VERSION 3.12)
project(citation_graph)

set(CMAKE_CXX_STANDARD 17)

add_executable(citation_graph citation_graph_example.cc)
add_executable(citation_graph_unit_tests citation_graph_unit_tests.cc)