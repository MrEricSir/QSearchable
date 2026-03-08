cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(QSearchableListDemo
    main.cpp
    ListDemoWindow.cpp
    ListDemoWindow.h
)

set_target_properties(QSearchableListDemo PROPERTIES
    MACOSX_BUNDLE TRUE
)

target_link_libraries(QSearchableListDemo PRIVATE
    QSearchable
    Qt6::Widgets
)

if(APPLE)
    add_custom_command(TARGET QSearchableListDemo POST_BUILD
        COMMAND codesign --sign - --force --deep "$<TARGET_BUNDLE_DIR:QSearchableListDemo>"
        COMMENT "Ad-hoc code signing QSearchableListDemo.app"
    )
endif()
