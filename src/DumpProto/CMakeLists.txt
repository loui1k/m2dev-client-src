file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

# Use tea.cpp from EterBase instead of maintaining a local copy
list(APPEND FILE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../EterBase/tea.cpp")

add_executable(DumpProto ${FILE_SOURCES})
set_target_properties(DumpProto PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_include_directories(DumpProto PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../EterBase
)

target_link_libraries(DumpProto
	lzo2
	sodium
)
