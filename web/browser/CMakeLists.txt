project(browser CXX)

## Browser-Simulator

## Find libtidy which is needed to parse HTML
find_package(TIDY REQUIRED)
find_package(Boost REQUIRED)
find_package(EVENT2 REQUIRED)
find_package(SPDYLAY REQUIRED)
find_package(GLIB REQUIRED)
find_package(OPENSSL REQUIRED)

include_directories(AFTER ${GLIB_INCLUDES} ${TIDY_INCLUDES} ${SPDYLAY_INCLUDES} ${OPENSSL_INCLUDES} ${EVENT2_INCLUDES} ${CMAKE_SOURCE_DIR}/web/utility)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(browser_sources
    browser.cc 
    ../utility/connection_manager.cc 
    ../utility/connection.cc 
    ../utility/request.cc 
    ../utility/shd-html.cc
    ../utility/shd-url.c
    ../utility/myevent.cc
    ../utility/common.cc
    ../utility/http_parse.c
)

## service library to allow browser to be used by any plugin
add_library(shadow-service-browser STATIC ${browser_sources})
add_dependencies(shadow-service-browser shadow-util)
target_link_libraries(shadow-service-browser ${RT_LIBRARIES} ${GLIB_LIBRARIES} ${TIDY_LIBRARIES} stdc++ ${SPDYLAY_LIBRARIES} ${OPENSSL_LIBRARIES} ${EVENT2_LIBRARIES})

# ## executable that can run outside of shadow
# add_executable(shadow-browser shd-browser-main.cc)
# target_link_libraries(shadow-browser shadow-service-browser ${RT_LIBRARIES} ${GLIB_LIBRARIES} ${TIDY_LIBRARIES} stdc++ ${SPDYLAY_LIBRARIES} ${OPENSSL_LIBRARIES} ${EVENT2_LIBRARIES})
# install(TARGETS shadow-browser DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-browser-bitcode ${browser_sources})
add_bitcode(shadow-plugin-browser-bitcode shd-browser-plugin.cc)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-browser shadow-plugin-browser-bitcode shadow-service-browser-bitcode)
target_link_libraries(shadow-plugin-browser ${TIDY_LIBRARIES} ${GLIB_LIBRARIES} ${TIDY_LIBRARIES} stdc++ ${SPDYLAY_LIBRARIES} ${OPENSSL_LIBRARIES} ${EVENT2_LIBRARIES})
install(TARGETS shadow-plugin-browser DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-browser shadow-service-browser-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
