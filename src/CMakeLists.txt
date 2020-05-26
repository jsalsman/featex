# Add all the src cpp files with the headers
add_executable(featex.o featex.c ${HEADER_LIST})

# Ensure everything in `include` is included.
target_include_directories(featex.o PUBLIC ../include)
target_include_directories(featex.o PUBLIC "/usr/local/include/pocketsphinx")
target_include_directories(featex.o PUBLIC "/usr/local/include/sphinxbase")

target_link_libraries(featex.o pocketsphinx sphinxbase m)

# IDEs should put the headers in a nice place
source_group(TREE "${PROJECT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADER_LIST})
