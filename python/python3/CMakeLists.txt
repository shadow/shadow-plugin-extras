## python plug-in

if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${GLIB_INCLUDES})
include_directories(/usr/include/python3.5m)
find_library(PYTHON3 python3.5m)

## library to manage global interpreter lock
add_library(shadow-python3-global-lock SHARED ../src/python-global-lock.c)
target_link_libraries(shadow-python3-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-python3-global-lock DESTINATION lib)

add_definitions(-DINSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

#
# Python 3 part
#
add_shadow_plugin(shadow-plugin-python3 ../src/python-main.c ../src/python.c ../src/python-logging.c)
target_link_libraries(shadow-plugin-python3 ${PYTHON3} shadow-python3-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-python3 DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(shadow-python3 ../src/python-main.c ../src/python.c ../src/python-logging.c)
add_dependencies(shadow-python3 shadow-python3-global-lock)
target_link_libraries(shadow-python3 ${PYTHON3} shadow-python3-global-lock ${GLIB_LIBRARIES})
install(TARGETS shadow-python3 DESTINATION bin)
