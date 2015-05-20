## python plug-in

#
# Python 3 part
#
include_directories(/usr/include/python3.4m)
find_library(PYTHON3 python3.4m)
add_bitcode(shadow-plugin-python3-bitcode ../src/python-plugin.c ../src/python.c)
add_plugin(shadow-plugin-python3 shadow-plugin-python-bitcode)
target_link_libraries(shadow-plugin-python3 ${PYTHON3})
install(TARGETS shadow-plugin-python3 DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(shadow-python3 ../src/python-main.c ../src/python.c)
target_link_libraries(shadow-python3 ${PYTHON3})
install(TARGETS shadow-python3 DESTINATION bin)
