file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(SpeedTreeLib STATIC ${FILE_SOURCES})

target_link_libraries(SpeedTreeLib
	mio
)

GroupSourcesByFolder(SpeedTreeLib)
