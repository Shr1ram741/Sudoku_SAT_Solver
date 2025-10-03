cmake_minimum_required(VERSION 3.10)
project(SudokuSATSolver)
set(CMAKE_CXX_STANDARD 17)

add_executable(sudoku_sat_solver
    src/main.cpp
    src/sat_solver.cpp
    src/cnf_parser.cpp
)

target_include_directories(sudoku_sat_solver PRIVATE include)
