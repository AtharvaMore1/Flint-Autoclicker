cmake_minimum_required(VERSION 3.21)
project(FlintAutoClicker VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(WIN32)
    enable_language(RC)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Qml QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(FlintAutoClicker WIN32
    src/main.cpp
    src/Backend.cpp
    src/Backend.hpp
    src/MacroBackend.cpp
    src/MacroBackend.hpp
)

if(WIN32)
    target_sources(FlintAutoClicker PRIVATE assets/app.rc)
    set_source_files_properties(assets/app.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.ico")
    target_include_directories(FlintAutoClicker PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()

qt_add_resources(FlintAutoClicker app_icon
    PREFIX "/icons"
    BASE "assets"
    FILES assets/app.ico
)

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/FlintButton.qml PROPERTIES QT_RESOURCE_ALIAS FlintButton.qml)
set_source_files_properties(qml/SegmentedControl.qml PROPERTIES QT_RESOURCE_ALIAS SegmentedControl.qml)
set_source_files_properties(qml/MacroPanel.qml PROPERTIES QT_RESOURCE_ALIAS MacroPanel.qml)
set_source_files_properties(qml/FlintComboBox.qml PROPERTIES QT_RESOURCE_ALIAS FlintComboBox.qml)
set_source_files_properties(qml/FileGlyph.qml PROPERTIES QT_RESOURCE_ALIAS FileGlyph.qml)
set_source_files_properties(qml/MouseGlyph.qml PROPERTIES QT_RESOURCE_ALIAS MouseGlyph.qml)
set_source_files_properties(qml/FlintCard.qml PROPERTIES QT_RESOURCE_ALIAS FlintCard.qml)
set_source_files_properties(qml/NumberField.qml PROPERTIES QT_RESOURCE_ALIAS NumberField.qml)

qt_add_qml_module(FlintAutoClicker
    URI Flint
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/FlintButton.qml
        qml/SegmentedControl.qml
        qml/MacroPanel.qml
        qml/FlintComboBox.qml
        qml/FileGlyph.qml
        qml/MouseGlyph.qml
        qml/FlintCard.qml
        qml/NumberField.qml
)

target_link_libraries(FlintAutoClicker PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    user32
)

option(FLINT_BUILD_TESTS "Build macro engine and UI checks" OFF)
if(FLINT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(MacroTests tests/MacroTests.cpp src/Backend.cpp src/Backend.hpp src/MacroBackend.cpp src/MacroBackend.hpp)
    target_include_directories(MacroTests PRIVATE src)
    target_compile_definitions(MacroTests PRIVATE FLINT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(MacroTests PRIVATE Qt6::Test Qt6::Quick Qt6::Qml Qt6::QuickControls2 user32)
    add_test(NAME MacroTests COMMAND MacroTests -o macro-tests.txt,txt)
    set_tests_properties(MacroTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
