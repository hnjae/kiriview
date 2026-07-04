set(scan_roots "${IMAGEVIEWPORT_SOURCE_DIR}/src" "${IMAGEVIEWPORT_SOURCE_DIR}/tests")
set(disallowed_includes)

foreach(scan_root IN LISTS scan_roots)
    file(
        GLOB_RECURSE scan_files
        LIST_DIRECTORIES false
        "${scan_root}/*.cpp"
        "${scan_root}/*.h"
        "${scan_root}/*.cmake"
        "${scan_root}/CMakeLists.txt")
    foreach(scan_file IN LISTS scan_files)
        file(READ "${scan_file}" scan_content)
        if(scan_content MATCHES "#[ \t]*include[ \t]*\"imageviewporthelpers_p\\.h\"")
            list(APPEND disallowed_includes "${scan_file}")
        endif()
    endforeach()
endforeach()

if(disallowed_includes)
    list(JOIN disallowed_includes "\n  " include_report)
    message(
        FATAL_ERROR
            "Subsystems must include owner-specific helper headers instead of imageviewporthelpers_p.h:\n  ${include_report}"
    )
endif()
