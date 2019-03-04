## tor-echo plug-in

add_cflags("-std=gnu99")

## create and install a shared library that can plug into shadow

## tor-echo-client
add_shadow_plugin(shadow-plugin-tor-echo-client tor-echo-client.c)
target_link_libraries(shadow-plugin-tor-echo-client)
install(TARGETS shadow-plugin-tor-echo-client DESTINATION plugins)

## tor-echo-server
add_shadow_plugin(shadow-plugin-tor-echo-server tor-echo-server.c)
target_link_libraries(shadow-plugin-tor-echo-server)
install(TARGETS shadow-plugin-tor-echo-server DESTINATION plugins)

## create and install an executable that can run outside of shadow

## tor-echo-client executable
add_executable(tor-echo-client tor-echo-client.c)
target_link_libraries(tor-echo-client)
install(TARGETS tor-echo-client DESTINATION bin)

## tor-echo-server executable
add_executable(tor-echo-server tor-echo-server.c)
target_link_libraries(tor-echo-server)
install(TARGETS tor-echo-server DESTINATION bin)
