## hello world plug-in dependencies
find_package(GLIB REQUIRED)

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-hello-bitcode hello-plugin.c)
add_plugin(shadow-plugin-hello shadow-plugin-hello-bitcode)
target_link_libraries(shadow-plugin-hello ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-hello DESTINATION plugins)

## create and install an executable that can run outside of shadow
#add_executable(hello hello-main.c)
#target_link_libraries(hello ${GLIB_LIBRARIES})
#install(TARGETS hello DESTINATION bin)
