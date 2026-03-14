find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_qsearchableindex tst_qsearchableindex.cpp)
target_link_libraries(tst_qsearchableindex PRIVATE QSearchable Qt6::Test)
set_target_properties(tst_qsearchableindex PROPERTIES WIN32_EXECUTABLE FALSE)
if(WIN32)
    target_link_options(tst_qsearchableindex PRIVATE "/SUBSYSTEM:CONSOLE")
endif()
add_test(NAME QSearchableIndex COMMAND tst_qsearchableindex)

# SearchableItemStore is compiled into the library only on Linux, but the
# class is pure Qt with no platform dependencies. Include the source
# directly so the test builds on all platforms.
add_executable(tst_searchableitemstore
    tst_searchableitemstore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends/SearchableItemStore.cpp
)
target_include_directories(tst_searchableitemstore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(tst_searchableitemstore PRIVATE QSearchable Qt6::Test)
set_target_properties(tst_searchableitemstore PROPERTIES WIN32_EXECUTABLE FALSE)
if(WIN32)
    target_link_options(tst_searchableitemstore PRIVATE "/SUBSYSTEM:CONSOLE")
endif()
add_test(NAME SearchableItemStore COMMAND tst_searchableitemstore)
