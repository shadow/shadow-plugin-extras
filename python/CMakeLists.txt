## python plug-in

find_package(GLIB REQUIRED)

#include_directories(/usr/include/python2.7)
include_directories(/home/javex/.shadow/include/python2.7)
#find_library(PYTHON python2.7)
find_library(PYTHON python2.7 /home/javex/.shadow/lib/ NO_DEFAULT_PATH)


## library to manage global interpreter lock
add_library(shadow-python-global-lock SHARED src/python-global-lock.c)
target_link_libraries(shadow-python-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-python-global-lock DESTINATION lib)


add_definitions(-DINSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

#link_directories(/home/javex/.shadow/lib/)
## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-python-bitcode src/python-plugin.c src/python.c src/python-logging.c)
add_plugin(shadow-plugin-python shadow-plugin-python-bitcode)
add_dependencies(shadow-plugin-python shadow-python-global-lock)
target_link_libraries(shadow-plugin-python ${PYTHON} shadow-python-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-python DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(shadow-python src/python-main.c src/python.c src/python-logging.c)
add_dependencies(shadow-python shadow-python-global-lock)
message(${PYTHON})
target_link_libraries(shadow-python ${PYTHON} shadow-python-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-python DESTINATION bin)

set_target_properties(shadow-plugin-python shadow-python PROPERTIES INSTALL_RPATH "/home/javex/.shadow/lib" BUILD_WITH_INSTALL_RPATH TRUE)

# Preload lib to intercept functions
#add_library(shadow-preload-python SHARED src/python-preload.c)
#target_link_libraries(shadow-preload-python
#    ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${RT_LIBRARIES} ${DL_LIBRARIES}
#)
#install(TARGETS shadow-preload-python DESTINATION lib)
