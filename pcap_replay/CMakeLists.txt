# pcap_replay - a minimal program re-play a pcap file 
project(shadow-plugin-pcap_replay)
cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)

# dependencies
find_package(GLIB REQUIRED)
include_directories(AFTER ${GLIB_INCLUDES})

## plug-ins need to disable fortification to ensure syscalls are intercepted
add_cflags("-fPIC -fno-inline -fno-strict-aliasing -U_FORTIFY_SOURCE")

## create and install a dynamic library that can plug into shadow
add_shadow_plugin(shadow-plugin-pcap_replay pcap_replay-main.c pcap_replay.c)
target_link_libraries(shadow-plugin-pcap_replay ${GLIB_LIBRARIES} -lpcap)
install(TARGETS shadow-plugin-pcap_replay DESTINATION plugins)

## create exe for testing
add_shadow_exe(shadow-plugin-pcap_replay-exe pcap_replay-main.c pcap_replay.c)
target_link_libraries(shadow-plugin-pcap_replay-exe ${GLIB_LIBRARIES} -lpcap)
