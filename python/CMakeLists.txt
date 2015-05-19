## python plug-in

include_directories(/home/javex/.shadow/include/python2.7)
find_library(PYTHON python2.7 /home/javex/.shadow/lib/)
## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-python-bitcode src/python-plugin.c src/python.c)
add_plugin(shadow-plugin-python shadow-plugin-python-bitcode)
target_link_libraries(shadow-plugin-python ${PYTHON})
install(TARGETS shadow-plugin-python DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(shadow-python src/python-main.c src/python.c)
target_link_libraries(shadow-python ${PYTHON})
install(TARGETS shadow-python DESTINATION bin)

set_target_properties(shadow-plugin-python shadow-python PROPERTIES INSTALL_RPATH "/home/javex/.shadow/lib" BUILD_WITH_INSTALL_RPATH TRUE)

# Preload lib to intercept functions
#add_library(shadow-preload-python SHARED src/python-preload.c)
#target_link_libraries(shadow-preload-python
#    ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${RT_LIBRARIES} ${DL_LIBRARIES}
#)
#install(TARGETS shadow-preload-python DESTINATION lib)