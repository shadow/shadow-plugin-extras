project(webserver CXX)

## WEBSERVER-Simulator

## Find libtidy which is needed to parse HTML
find_package(EVENT2 REQUIRED)

include_directories(AFTER ${EVENT2_INCLUDES} ${CMAKE_SOURCE_DIR}/utility)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -UDEBUG_PIPELINE")

set(webserver_sources
    webserver.cc
    handler.cc
    ../utility/myevent.cc
    ../utility/common.cc
    ../utility/http_parse.c
)

## service library to allow webserver to be used by any plugin
add_library(shadow-service-webserver STATIC ${webserver_sources})
add_dependencies(shadow-service-webserver shadow-util)
target_link_libraries(shadow-service-webserver ${RT_LIBRARIES} stdc++ ${EVENT2_LIBRARIES})

# ## executable that can run outside of shadow
# add_executable(shadow-webserver shd-webserver-main.cc)
# target_link_libraries(shadow-webserver shadow-service-webserver ${RT_LIBRARIES} stdc++ ${EVENT2_LIBRARIES})
# install(TARGETS shadow-webserver DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-webserver-bitcode ${webserver_sources})
add_bitcode(shadow-plugin-webserver-bitcode shd-webserver-plugin.cc)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-webserver shadow-plugin-webserver-bitcode shadow-service-webserver-bitcode)
target_link_libraries(shadow-plugin-webserver stdc++ ${EVENT2_LIBRARIES})
install(TARGETS shadow-plugin-webserver DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-webserver shadow-service-webserver-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
