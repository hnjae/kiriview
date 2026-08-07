# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

project(
    KiriView
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KIRIVIEW_BUILD_TESTS "Build KiriView tests" ON)
option(KIRIVIEW_CARGO_OFFLINE "Require Cargo to use only locally available sources" OFF)
set(KIRIVIEW_QML_IMPORT_PATHS
    ""
    CACHE STRING "Additional QML import paths for development tooling")

set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/application-id.txt")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/application-id.txt" kiriview_application_id_lines)
set(kiriview_application_id_values)
foreach(line IN LISTS kiriview_application_id_lines)
    if(NOT line MATCHES "^[ \t]*$" AND NOT line MATCHES "^[ \t]*#")
        list(APPEND kiriview_application_id_values "${line}")
    endif()
endforeach()
list(LENGTH kiriview_application_id_values kiriview_application_id_count)
if(NOT kiriview_application_id_count EQUAL 1)
    message(FATAL_ERROR "application-id.txt must declare exactly one application ID")
endif()
list(GET kiriview_application_id_values 0 KIRIVIEW_APPLICATION_ID)
if(NOT KIRIVIEW_APPLICATION_ID MATCHES "^[a-z][a-z0-9]*(\\.[a-z][a-z0-9]*)+$")
    message(FATAL_ERROR "Invalid application ID: ${KIRIVIEW_APPLICATION_ID}")
endif()
string(REPLACE "." "/" KIRIVIEW_QML_MODULE_PATH "${KIRIVIEW_APPLICATION_ID}")
set(KIRIVIEW_DESKTOP_FILE "${KIRIVIEW_APPLICATION_ID}.desktop")
set(KIRIVIEW_ICON_FILE "${KIRIVIEW_APPLICATION_ID}.svg")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/application/applicationidentity.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/generated/applicationidentity.h" @ONLY)

find_package(
    Qt6 6.11 REQUIRED
    COMPONENTS Core
               DBus
               Gui
               Multimedia
               Qml
               Quick
               QuickControls2
               Widgets)
qt_standard_project_setup(REQUIRES 6.11)
add_compile_definitions(QT_ENABLE_STRICT_MODE_UP_TO=0x060b00 QT_NO_KEYWORDS)
find_package(KF6Archive CONFIG REQUIRED)
find_package(KF6Config CONFIG REQUIRED)
find_package(KF6CoreAddons CONFIG REQUIRED)
find_package(KF6I18n CONFIG REQUIRED)
find_package(KF6KirigamiAddons CONFIG REQUIRED)
find_package(KF6KIO CONFIG REQUIRED)
find_package(KF6JobWidgets CONFIG REQUIRED)
find_package(KF6Service CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LibArchive REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(LibHeif REQUIRED IMPORTED_TARGET libheif)
pkg_check_modules(LibJxl REQUIRED IMPORTED_TARGET libjxl)
pkg_check_modules(LibJxlThreads REQUIRED IMPORTED_TARGET libjxl_threads)
pkg_check_modules(LibRaw REQUIRED IMPORTED_TARGET libraw)
pkg_check_modules(LibWebP REQUIRED IMPORTED_TARGET libwebp)
pkg_check_modules(LibWebPDemux REQUIRED IMPORTED_TARGET libwebpdemux)

# Read a source manifest into the caller-provided output variable.
function(kiriview_read_source_manifest output manifest)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
    file(STRINGS "${manifest}" entries)
    set(sources)
    foreach(entry IN LISTS entries)
        string(STRIP "${entry}" entry)
        if(entry STREQUAL "" OR entry MATCHES "^#")
            continue()
        endif()
        if(IS_ABSOLUTE "${entry}" OR entry MATCHES "(^|/)\.\.(/|$)")
            message(FATAL_ERROR "Invalid source entry ${entry} in ${manifest}")
        endif()
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${entry}")
            message(FATAL_ERROR "Missing source entry ${entry} in ${manifest}")
        endif()
        list(APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/${entry}")
    endforeach()
    set("${output}"
        "${sources}"
        PARENT_SCOPE)
endfunction()

kiriview_read_source_manifest(kiriview_core_sources
                              "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_core_sources.txt")
kiriview_read_source_manifest(kiriview_qml_cpp_sources
                              "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_qml_sources.txt")
kiriview_read_source_manifest(kiriview_qml_headers
                              "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_qml_header_sources.txt")
file(GLOB_RECURSE kiriview_rust_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/support/*.rs")

set(KIRIVIEW_IMAGE_VIEWPORT_SOURCE_DIR
    ""
    CACHE PATH "ImageViewport source directory")
if(NOT KIRIVIEW_IMAGE_VIEWPORT_SOURCE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libs/image-viewport/CMakeLists.txt")
        set(KIRIVIEW_IMAGE_VIEWPORT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs/image-viewport")
    else()
        set(KIRIVIEW_IMAGE_VIEWPORT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/image-viewport")
    endif()
endif()
set(IMAGEVIEWPORT_BUILD_TESTS
    OFF
    CACHE BOOL "Build ImageViewport tests as part of the application" FORCE)
add_subdirectory("${KIRIVIEW_IMAGE_VIEWPORT_SOURCE_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/image-viewport")

set(KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_SOURCE_DIR
    ""
    CACHE PATH "VideoThumbnailExtraction source directory")
if(NOT KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_SOURCE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libs/video-thumbnail-extraction/CMakeLists.txt")
        set(KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/../libs/video-thumbnail-extraction")
    else()
        set(KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/libs/video-thumbnail-extraction")
    endif()
endif()
set(VIDEO_THUMBNAIL_EXTRACTION_BUILD_TESTS
    OFF
    CACHE BOOL "Build VideoThumbnailExtraction tests as part of the application" FORCE)
add_subdirectory("${KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_SOURCE_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/video-thumbnail-extraction")

find_program(CARGO_EXECUTABLE cargo REQUIRED)
set(KIRIVIEW_CARGO_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo")
set(KIRIVIEW_CXXBRIDGE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cxxbridge")
set(KIRIVIEW_CXXBRIDGE_TRIGGER "${CMAKE_CURRENT_BINARY_DIR}/rust-bridge.trigger")
set(kiriview_cargo_arguments build --locked --lib)
if(KIRIVIEW_CARGO_OFFLINE)
    list(APPEND kiriview_cargo_arguments --offline)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND kiriview_cargo_arguments --release)
    set(kiriview_rust_profile release)
else()
    set(kiriview_rust_profile debug)
endif()
set(kiriview_rust_archive "${KIRIVIEW_CARGO_TARGET_DIR}/${kiriview_rust_profile}/libkiriview.a")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/rust_support_bridge_sources.txt"
     kiriview_rust_bridge_entries)
set(kiriview_generated_bridge_sources)
set(kiriview_generated_bridge_headers)
foreach(entry IN LISTS kiriview_rust_bridge_entries)
    string(STRIP "${entry}" entry)
    if(entry STREQUAL "" OR entry MATCHES "^#")
        continue()
    endif()
    string(REGEX REPLACE "^src/" "" bridge_relative "${entry}")
    string(REGEX REPLACE "\\.rs$" ".cxx.cpp" bridge_relative "${bridge_relative}")
    list(APPEND kiriview_generated_bridge_sources
         "${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}/sources/${bridge_relative}")
    string(REGEX REPLACE "^src/" "" bridge_header_relative "${entry}")
    string(REGEX REPLACE "\\.rs$" ".cxx.h" bridge_header_relative "${bridge_header_relative}")
    list(APPEND kiriview_generated_bridge_headers
         "${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}/kiriview/src/${bridge_header_relative}")
endforeach()

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rust-bridge.stamp" "${kiriview_rust_archive}"
           "${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}/rust/cxx.h" ${kiriview_generated_bridge_headers}
           ${kiriview_generated_bridge_sources}
    COMMAND "${CMAKE_COMMAND}" -E touch "${KIRIVIEW_CXXBRIDGE_TRIGGER}"
    COMMAND
        "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${KIRIVIEW_CARGO_TARGET_DIR}"
        "KIRIVIEW_CXXBRIDGE_OUTPUT_DIR=${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}"
        "KIRIVIEW_CXXBRIDGE_TRIGGER=${KIRIVIEW_CXXBRIDGE_TRIGGER}" "${CARGO_EXECUTABLE}"
        ${kiriview_cargo_arguments}
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/rust-bridge.stamp"
    DEPENDS Cargo.toml Cargo.lock build.rs src/lib.rs src/rust_support_bridge_sources.txt
            ${kiriview_rust_sources}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Build the Rust support library and CXX bridges"
    VERBATIM)
add_custom_target(
    KiriViewRustSupportArtifacts
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rust-bridge.stamp" "${kiriview_rust_archive}"
            "${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}/rust/cxx.h" ${kiriview_generated_bridge_headers}
            ${kiriview_generated_bridge_sources}
    COMMENT "Build the Rust support artifacts")

add_library(KiriViewRustSupportArchive STATIC IMPORTED GLOBAL)
set_property(TARGET KiriViewRustSupportArchive PROPERTY IMPORTED_LOCATION
                                                        "${kiriview_rust_archive}")
add_dependencies(KiriViewRustSupportArchive KiriViewRustSupportArtifacts)

set_source_files_properties(${kiriview_generated_bridge_sources} PROPERTIES GENERATED TRUE)
add_library(KiriViewRustSupportBridge STATIC ${kiriview_generated_bridge_sources})
add_dependencies(KiriViewRustSupportBridge KiriViewRustSupportArtifacts)
target_compile_features(KiriViewRustSupportBridge PUBLIC cxx_std_23)
set_target_properties(KiriViewRustSupportBridge PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(KiriViewRustSupportBridge PUBLIC "${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}")
target_link_libraries(KiriViewRustSupportBridge PUBLIC KiriViewRustSupportArchive)

add_library(KiriViewCore STATIC ${kiriview_core_sources} ${kiriview_qml_cpp_sources})
target_compile_features(KiriViewCore PUBLIC cxx_std_23)
set_target_properties(KiriViewCore PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    KiriViewCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" "${KIRIVIEW_CXXBRIDGE_OUTPUT_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(KiriViewCore PUBLIC TRANSLATION_DOMAIN="kiriview")
target_link_libraries(
    KiriViewCore
    PUBLIC ImageViewport
           VideoThumbnailExtraction
           Qt6::Core
           Qt6::Gui
           Qt6::Qml
           Qt6::Quick
           KF6::ConfigCore
           KF6::ConfigGui
           KF6::CoreAddons
           KF6::I18n
           KF6::I18nQml
           KF6::KIOCore
           KF6::KIOGui
           KF6::KIOWidgets
           KF6::JobWidgets
           KF6::Service
           KirigamiAddonsStatefulApp
    PRIVATE KiriViewRustSupportBridge
            Qt6::DBus
            Qt6::Multimedia
            Qt6::QuickControls2
            Qt6::Widgets
            KF6::Archive
            PkgConfig::LibArchive
            PkgConfig::LibHeif
            PkgConfig::LibJxl
            PkgConfig::LibJxlThreads
            PkgConfig::LibRaw
            PkgConfig::LibWebP
            PkgConfig::LibWebPDemux)
kconfig_add_kcfg_files(KiriViewCore src/kiriviewstate.kcfgc)

set(kiriview_qml_files
    src/qml/ActionProxy.qml
    src/qml/ApplicationMenuBar.qml
    src/qml/ContextActionMenu.qml
    src/qml/FitModeMenuButton.qml
    src/qml/ImageActions.qml
    src/qml/ImageDocumentPageNavigation.qml
    src/qml/ImageDocumentToolBar.qml
    src/qml/ImageStateOverlay.qml
    src/qml/ImageToolBar.qml
    src/qml/ImageToolBarMenuCoordinator.qml
    src/qml/ImageToolBarRetainedPresentation.qml
    src/qml/ImageToolBarTextInputCoordinator.qml
    src/qml/ImageViewport.qml
    src/qml/ImageZoomControls.qml
    src/qml/InfoPanel.qml
    src/qml/Main.qml
    src/qml/ManagedAction.qml
    src/qml/MediaViewportDelegate.qml
    src/qml/MediaViewportHost.qml
    src/qml/MediaWorkspaceHost.qml
    src/qml/MenuActionItem.qml
    src/qml/NavigationPresentationOrder.qml
    src/qml/ShortcutDelegate.qml
    src/qml/ShortcutHelpDialog.qml
    src/qml/ThumbnailPanel.qml
    src/qml/ToastNotification.qml
    src/qml/VideoFloatingControls.qml
    src/qml/VideoViewport.qml
    src/qml/ZoomWheelStepPolicy.qml)
set_source_files_properties(
    src/qml/ImageToolBarMenuCoordinator.qml src/qml/ImageToolBarRetainedPresentation.qml
    src/qml/ImageToolBarTextInputCoordinator.qml PROPERTIES QT_QML_INTERNAL_TYPE TRUE)
foreach(qml_file IN LISTS kiriview_qml_files)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

qt_add_library(KiriViewQml STATIC)
qt_add_qml_module(
    KiriViewQml
    URI
    ${KIRIVIEW_APPLICATION_ID}
    IMPORT_PATH
    ${KIRIVIEW_QML_IMPORT_PATHS}
    DEPENDENCIES
    QtQuick
    org.kde.kirigamiaddons.statefulapp
    OUTPUT_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/${KIRIVIEW_QML_MODULE_PATH}"
    RESOURCE_PREFIX
    /qt/qml
    QML_FILES
    ${kiriview_qml_files}
    SOURCES
    ${kiriview_qml_headers})
target_compile_features(KiriViewQml PUBLIC cxx_std_23)
set_target_properties(KiriViewQml PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(KiriViewQml PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/facade")
target_link_libraries(KiriViewQml PUBLIC KiriViewCore)

qt_add_executable(kiriview src/main.cpp)
target_compile_features(kiriview PRIVATE cxx_std_23)
set_target_properties(kiriview PROPERTIES CXX_EXTENSIONS OFF QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
target_link_libraries(kiriview PRIVATE KiriViewQml KiriViewQmlplugin)

include(GNUInstallDirs)
install(TARGETS kiriview RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "data/icons/hicolor/scalable/apps/${KIRIVIEW_ICON_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
install(FILES "${KIRIVIEW_DESKTOP_FILE}" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

if(KIRIVIEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
