## example echo server/client plug-in

if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${GLIB_INCLUDES})

## executable that can run outside of shadow
add_executable(echo echo-main.c echo-tcp.c echo-udp.c echo-pipe.c)
target_link_libraries(echo ${GLIB_LIBRARIES})
install(TARGETS echo DESTINATION bin)

## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-echo echo-main.c echo-tcp.c echo-udp.c echo-pipe.c)
target_link_libraries(shadow-plugin-echo ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-echo DESTINATION plugins)
