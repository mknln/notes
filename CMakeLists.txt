cmake_minimum_required(VERSION 3.21)
# Keep the version in step with debian/changelog.
project(Notes VERSION 0.1.0 LANGUAGES CXX)

# Qt is often installed somewhere CMake doesn't look, so this explains how to
# point it there, rather than find_package()'s more general error.
find_package(Qt6 QUIET)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Qt 6 wasn't found. Set CMAKE_PREFIX_PATH to the Qt installation's folder "
        "for your compiler, e.g. -DCMAKE_PREFIX_PATH=~/Qt/6.8.3/gcc_64, or use a "
        "preset from a CMakeUserPresets.json that sets it. See \"Building\" in "
        "README.md.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2)
# CorePrivate is for QZipReader and QZipWriter, which Qt has no public
# equivalent of. Qt 6.9 moved it into a package of its own; before that, Core
# provides it.
if(NOT TARGET Qt6::CorePrivate)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
endif()

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(qtnotes WIN32 MACOSX_BUNDLE
    main.cpp
)

qt_add_qml_module(qtnotes
    URI Notes
    QML_FILES
        Main.qml
        NoteListDelegate.qml
        ShortcutsDialog.qml
    SOURCES
        notesmodel.h notesmodel.cpp
        textformatter.h textformatter.cpp
)

set_target_properties(qtnotes PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME QtNotes
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.mknln.qtnotes
)

target_link_libraries(qtnotes PRIVATE
    Qt6::Core
    Qt6::CorePrivate
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

# Lets QML debuggers and profilers, such as Qt Creator's, connect to builds
# with debug info. They only listen when the app is started with
# -qmljsdebugger.
target_compile_definitions(qtnotes PRIVATE
    $<$<CONFIG:Debug,RelWithDebInfo>:QT_QML_DEBUG>
)

include(GNUInstallDirs)
install(TARGETS qtnotes
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# Lists the app in Linux desktops' application menus.
if(UNIX AND NOT APPLE)
    install(FILES linux/io.github.mknln.qtnotes.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
endif()

# Copies the Qt libraries, plugins and QML modules the app needs next to it
# when it's installed, so it runs on machines without Qt. Linux builds usually
# use the system's Qt instead, so it's off there by default.
if(WIN32 OR APPLE)
    set(deploy_default ON)
else()
    set(deploy_default OFF)
endif()
option(QTNOTES_DEPLOY_QT "Install the Qt libraries the app needs along with it" ${deploy_default})
if(QTNOTES_DEPLOY_QT)
    qt_generate_deploy_qml_app_script(
        TARGET qtnotes
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})
endif()
