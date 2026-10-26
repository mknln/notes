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

# Windows has no system copy of Microsoft's C++ runtime that's guaranteed to
# be new enough, so its DLLs are installed next to the app too.
if(WIN32 AND QTNOTES_DEPLOY_QT)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
endif()

# Packaging with CPack: "cmake --build <build folder> --target package" packs
# what "cmake --install" installs. On Windows, that's an installer made with
# NSIS (https://nsis.sourceforge.io), which needs to be installed. Linux uses
# the debian folder instead.
set(CPACK_PACKAGE_NAME QtNotes)
set(CPACK_PACKAGE_VENDOR "Mike Nolan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple notes app with Markdown formatting")
set(CPACK_PACKAGE_HOMEPAGE_URL https://github.com/mknln/notes)
# Installs to Program Files\QtNotes. CPack only picks the 64-bit Program Files
# folder by itself for Microsoft's compiler.
set(CPACK_PACKAGE_INSTALL_DIRECTORY QtNotes)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
endif()
# Without the version, so installing a new version finds and replaces the old
# one, rather than being listed separately in Settings > Apps.
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY QtNotes)
# A Start menu shortcut named QtNotes, to bin\qtnotes.exe.
set(CPACK_PACKAGE_EXECUTABLES qtnotes QtNotes)
if(WIN32)
    set(CPACK_GENERATOR NSIS)
endif()
set(CPACK_NSIS_PACKAGE_NAME QtNotes)
set(CPACK_NSIS_DISPLAY_NAME QtNotes)
# The icon shown in Settings > Apps. The backslash is escaped twice: once
# for here and once for the CPack configuration it's written to.
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\qtnotes.exe")
set(CPACK_NSIS_URL_INFO_ABOUT ${CPACK_PACKAGE_HOMEPAGE_URL})
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
# Offers to start the app when the installer finishes.
set(CPACK_NSIS_MUI_FINISHPAGE_RUN qtnotes.exe)
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
include(CPack)
