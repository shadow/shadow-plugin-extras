## P2P torrent transfers plug-in

if("${RT_LIBRARIES}" STREQUAL "")
    find_package(RT REQUIRED)
endif()
if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${RT_INCLUDES} ${GLIB_INCLUDES})

set(torrent_sources
    torrent-service.c
    torrent.c 
    torrent-server.c 
    torrent-client.c 
    torrent-authority.c
)

## service target to allow torrent service to be used by any plugin
add_library(shadow-service-torrent STATIC ${torrent_sources})
target_link_libraries(shadow-service-torrent ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(shadow-torrent torrent-main.c ${torrent_sources})
target_link_libraries(shadow-torrent ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-torrent DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-torrent-bitcode ${torrent_sources})
add_bitcode(shadow-plugin-torrent-bitcode torrent-plugin.c)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-torrent shadow-plugin-torrent-bitcode shadow-service-torrent-bitcode)
target_link_libraries(shadow-plugin-torrent ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-torrent DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-torrent-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
