## HTTP file server and client for transfering files through shadow

if("${RT_LIBRARIES}" STREQUAL "")
    find_package(RT REQUIRED)
endif()
if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${RT_INCLUDES} ${GLIB_INCLUDES})

set(filetransfer_sources
    filetransfer.c 
    fileserver.c 
    service-filegetter.c 
    filegetter.c
    cdf.c
)

## service target to allow filetransfer service to be used by any plugin
add_library(shadow-service-filetransfer STATIC ${filetransfer_sources})
add_dependencies(shadow-service-filetransfer shadow-util)
target_link_libraries(shadow-service-filetransfer ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(shadow-filetransfer filetransfer-main.c)
target_link_libraries(shadow-filetransfer shadow-service-filetransfer ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-filetransfer DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-filetransfer-bitcode ${filetransfer_sources})
add_bitcode(shadow-plugin-filetransfer-bitcode filetransfer-plugin.c)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-filetransfer shadow-plugin-filetransfer-bitcode shadow-service-filetransfer-bitcode)
target_link_libraries(shadow-plugin-filetransfer ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-filetransfer DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-filetransfer shadow-service-filetransfer-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)

## generate some files that filetransfer servers will serve
macro(ddexe ddfilename ddkib)
    set(ddfilepath ${CMAKE_CURRENT_BINARY_DIR}/${ddfilename})
    if(NOT EXISTS ${ddfilepath})
        message(STATUS "Generating ${ddfilepath}")
        execute_process(COMMAND "dd" "if=/dev/urandom" "of=${ddfilepath}" "bs=1024" "count=${ddkib}" RESULT_VARIABLE DDRESULT OUTPUT_QUIET ERROR_QUIET)
        if(NOT (${DDRESULT} EQUAL 0))
            message(FATAL_ERROR "dd failed. Is it installed and in your path?")
        endif(NOT (${DDRESULT} EQUAL 0))
    endif(NOT EXISTS ${ddfilepath})
    install(FILES ${ddfilepath} DESTINATION share/)
endmacro()

ddexe("1KiB.urnd" 1)
ddexe("16KiB.urnd" 16)
ddexe("32KiB.urnd" 32)
ddexe("50KiB.urnd" 50)
ddexe("320KiB.urnd" 320)
ddexe("1MiB.urnd" 1024)
ddexe("5MiB.urnd" 5120)
