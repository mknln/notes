cmake_minimum_required(VERSION 3.16)
project(Notes LANGUAGES CXX)

# CorePrivate is for QZipReader and QZipWriter, which Qt has no public
# equivalent of.
find_package(Qt6 REQUIRED COMPONENTS Core CorePrivate Gui Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(qtnotes WIN32 MACOSX_BUNDLE
    main.cpp
)

qt_add_qml_module(qtnotes
    URI Notes
    QML_FILES
        Main.qml
        NoteListDelegate.qml
    SOURCES
        notesmodel.h notesmodel.cpp
        textformatter.h textformatter.cpp
)

target_link_libraries(qtnotes PRIVATE
    Qt6::Core
    Qt6::CorePrivate
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS qtnotes
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
