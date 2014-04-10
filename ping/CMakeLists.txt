## ping plug-in

if("${RT_LIBRARIES}" STREQUAL "")
    find_package(RT REQUIRED)
endif()
if("${GLIB_LIBRARIES}" STREQUAL "")
    find_package(GLIB REQUIRED)
endif()
include_directories(${RT_INCLUDES} ${GLIB_INCLUDES})

set(ping_sources
    ping-service.c
    ping.c 
    ping-server.c 
    ping-client.c 
)

## service target to allow ping service to be used by any plugin
add_library(shadow-service-ping STATIC ${ping_sources})
target_link_libraries(shadow-service-ping ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(shadow-ping ping-main.c ${ping_sources})
target_link_libraries(shadow-ping ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-ping DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-ping-bitcode ${ping_sources})
add_bitcode(shadow-plugin-ping-bitcode ping-plugin.c)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-ping shadow-plugin-ping-bitcode shadow-service-ping-bitcode)
target_link_libraries(shadow-plugin-ping ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-ping DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-ping-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
