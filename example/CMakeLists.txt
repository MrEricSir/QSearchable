cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(APP_ICON_RESOURCE "")
if(WIN32)
    set(APP_ICON_RESOURCE app.rc)
elseif(APPLE)
    set(APP_ICON_RESOURCE app.icns)
endif()

qt_add_executable(QSearchableListDemo
    main.cpp
    ListDemoWindow.cpp
    ListDemoWindow.h
    resources.qrc
    ${APP_ICON_RESOURCE}
)

set_target_properties(QSearchableListDemo PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(APPLE)
    set_source_files_properties(app.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(QSearchableListDemo PROPERTIES MACOSX_BUNDLE_ICON_FILE app.icns)
endif()

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
