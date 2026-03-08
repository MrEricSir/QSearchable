find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_qsearchableindex tst_qsearchableindex.cpp)
target_link_libraries(tst_qsearchableindex PRIVATE QSearchable Qt6::Test)
add_test(NAME QSearchableIndex COMMAND tst_qsearchableindex)
