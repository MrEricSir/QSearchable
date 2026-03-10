find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_qsearchableindex tst_qsearchableindex.cpp)
target_link_libraries(tst_qsearchableindex PRIVATE QSearchable Qt6::Test)
set_target_properties(tst_qsearchableindex PROPERTIES WIN32_EXECUTABLE FALSE)
if(WIN32)
    target_link_options(tst_qsearchableindex PRIVATE "/SUBSYSTEM:CONSOLE")
endif()
add_test(NAME QSearchableIndex COMMAND tst_qsearchableindex)
