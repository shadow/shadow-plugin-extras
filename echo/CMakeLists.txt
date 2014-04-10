## example echo server/client plug-in

if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${GLIB_INCLUDES})

## executable that can run outside of shadow
add_executable(shadow-echo echo-main.c echo-tcp.c echo-udp.c echo-pipe.c)
target_link_libraries(shadow-echo ${GLIB_LIBRARIES})
install(TARGETS shadow-echo DESTINATION bin)

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-echo-bitcode 
    echo-plugin.c echo-tcp.c echo-udp.c echo-pipe.c)
add_plugin(shadow-plugin-echo shadow-plugin-echo-bitcode)
target_link_libraries(shadow-plugin-echo ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-echo DESTINATION plugins)
