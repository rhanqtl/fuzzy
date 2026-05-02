add_executable(ex_nqueens)
target_sources(ex_nqueens PRIVATE n-queens/main.cpp)
target_link_libraries(ex_nqueens PRIVATE fuzzy)
