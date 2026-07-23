# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Configure KiriView tests through the application CMake project")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
find_program(MSGFMT_EXECUTABLE msgfmt REQUIRED)

set(KIRIVIEW_TEST_LOCALE_DIR "${CMAKE_CURRENT_BINARY_DIR}/locale")
set(KIRIVIEW_TEST_KO_PO "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/i18n/ko/kiriview.po")
set(KIRIVIEW_TEST_KO_MO "${KIRIVIEW_TEST_LOCALE_DIR}/ko/LC_MESSAGES/kiriview.mo")

add_custom_command(
    OUTPUT "${KIRIVIEW_TEST_KO_MO}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${KIRIVIEW_TEST_LOCALE_DIR}/ko/LC_MESSAGES"
    COMMAND "${MSGFMT_EXECUTABLE}" --output-file "${KIRIVIEW_TEST_KO_MO}" "${KIRIVIEW_TEST_KO_PO}"
    DEPENDS "${KIRIVIEW_TEST_KO_PO}"
    VERBATIM)

add_custom_target(kiriview_test_i18n ALL DEPENDS "${KIRIVIEW_TEST_KO_MO}")

function(add_kiriview_cpp_test target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(
        ${target} PRIVATE KIRIVIEW_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
                          KIRIVIEW_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}")
    target_link_libraries(${target} PRIVATE KiriViewCore KiriViewQml KiriViewQmlplugin Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

file(GLOB KIRIVIEW_CPP_TEST_SOURCES CONFIGURE_DEPENDS "tst_*.cpp")
list(SORT KIRIVIEW_CPP_TEST_SOURCES)
foreach(test_source IN LISTS KIRIVIEW_CPP_TEST_SOURCES)
    get_filename_component(test_target "${test_source}" NAME_WE)
    add_kiriview_cpp_test("${test_target}" "${test_source}")
endforeach()

if(TARGET tst_localization)
    add_dependencies(tst_localization kiriview_test_i18n)
    target_compile_definitions(tst_localization
                               PRIVATE KIRIVIEW_TEST_LOCALE_DIR="${KIRIVIEW_TEST_LOCALE_DIR}")
    set_tests_properties(tst_localization PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_toastnotification)
    set_tests_properties(tst_toastnotification PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imageactions)
    set_tests_properties(tst_imageactions PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_menuaccesskeyrouter)
    set_tests_properties(tst_menuaccesskeyrouter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_menuactionitem)
    set_tests_properties(tst_menuactionitem PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_mainwindowtoolbar)
    set_tests_properties(tst_mainwindowtoolbar
                         PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2.075")
endif()

if(TARGET tst_mainwindowvideointegration)
    set_tests_properties(tst_mainwindowvideointegration PROPERTIES ENVIRONMENT
                                                                   "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imagezoomcontrols)
    set_tests_properties(tst_imagezoomcontrols PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imagestateoverlay)
    set_tests_properties(tst_imagestateoverlay PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_thumbnailpanel)
    set_tests_properties(tst_thumbnailpanel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_toolbarapplicationmenu)
    set_tests_properties(tst_toolbarapplicationmenu PROPERTIES ENVIRONMENT
                                                               "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imageshortcuts)
    set_tests_properties(tst_imageshortcuts PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_kiridocumentsession)
    set_tests_properties(tst_kiridocumentsession PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imageviewportcomponentboundary)
    set_tests_properties(tst_imageviewportcomponentboundary PROPERTIES ENVIRONMENT
                                                                       "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imageviewportsequenceprovider)
    set_tests_properties(tst_imageviewportsequenceprovider PROPERTIES ENVIRONMENT
                                                                      "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_imageviewportintegrationruntime)
    set_tests_properties(tst_imageviewportintegrationruntime PROPERTIES ENVIRONMENT
                                                                        "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_videodocumentruntimezoom)
    set_tests_properties(tst_videodocumentruntimezoom PROPERTIES ENVIRONMENT
                                                                 "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_videooutputruntime)
    set_tests_properties(tst_videooutputruntime PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_shortcuthelpdialog)
    set_tests_properties(tst_shortcuthelpdialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET tst_shortcuthelpmodel)
    set_tests_properties(tst_shortcuthelpmodel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
