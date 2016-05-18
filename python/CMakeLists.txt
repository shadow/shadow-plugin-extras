## python plug-in

if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${GLIB_INCLUDES})
include_directories(/usr/include/python2.7)
find_library(PYTHON python2.7)

## library to manage global interpreter lock
add_library(shadow-python-global-lock SHARED src/python-global-lock.c)
target_link_libraries(shadow-python-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-python-global-lock DESTINATION lib)

add_definitions(-DINSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

#
# Python part
#
add_shadow_plugin(shadow-plugin-python src/python-main.c src/python.c src/python-logging.c)
target_link_libraries(shadow-plugin-python ${PYTHON} shadow-python-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-python DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(shadow-python src/python-main.c src/python.c src/python-logging.c)
add_dependencies(shadow-python shadow-python-global-lock)
target_link_libraries(shadow-python ${PYTHON} shadow-python-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-python DESTINATION bin)
