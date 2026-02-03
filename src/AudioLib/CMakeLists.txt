file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(AudioLib STATIC ${FILE_SOURCES})

target_link_libraries(AudioLib
	mio
)

GroupSourcesByFolder(AudioLib)
