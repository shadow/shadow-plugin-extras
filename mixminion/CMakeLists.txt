## mixminion world plug-in

include_directories(/usr/include/python2.7)

## create and install a shared library that can plug into shadow
#add_bitcode(shadow-plugin-mixminion-bitcode src/mixminon-plugin.c src/mixminion.c)
#add_plugin(shadow-plugin-mixminion shadow-plugin-mixminion-bitcode)
#target_link_libraries(shadow-plugin-mixminion)
#install(TARGETS shadow-plugin-mixminion DESTINATION plugins)
file (COPY src/mixminion-runner.py DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
## create and install an executable that can run outside of shadow
add_executable(mixminion src/mixminion-main.c src/mixminion.c)
target_link_libraries(mixminion python2.7)
install(TARGETS mixminion DESTINATION bin)
