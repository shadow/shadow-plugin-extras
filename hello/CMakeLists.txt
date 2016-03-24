## hello world plug-in

## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-hello hello.c)
target_link_libraries(shadow-plugin-hello)
install(TARGETS shadow-plugin-hello DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(hello hello.c)
target_link_libraries(hello)
install(TARGETS hello DESTINATION bin)
