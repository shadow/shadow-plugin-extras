## mixminion world plug-in

include_directories(/usr/include/python3.4m)

## create and install a shared library that can plug into shadow
file (COPY src/smtp-runner.py DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
## create and install an executable that can run outside of shadow
add_executable(smtp src/smtp-main.c src/smtp.c)
target_link_libraries(smtp python3.4m)
install(TARGETS smtp DESTINATION bin)
