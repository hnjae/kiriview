set(scan_root "${IMAGEVIEWPORT_SOURCE_DIR}/src")

file(
    GLOB controller_files
    LIST_DIRECTORIES false
    "${scan_root}/viewportcontroller*.cpp")

set(violations)
foreach(controller_file IN LISTS controller_files)
    file(READ "${controller_file}" controller_content)
    if(controller_content MATCHES "#[ \t]*include[ \t]*\"viewportcontrollerhelpers_p\\.h\"")
        list(APPEND violations "${controller_file}")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Controller implementation units must include owner-specific helper headers instead of viewportcontrollerhelpers_p.h:\n  ${violation_report}"
    )
endif()
