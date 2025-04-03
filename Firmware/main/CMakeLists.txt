# Collect all source files
file(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

# Register the component and add the parent include directory
idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "../include")

# for bypassing not being able to send deauth packets
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)