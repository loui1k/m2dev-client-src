file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(PRTerrainLib STATIC ${FILE_SOURCES})

target_link_libraries(PRTerrainLib
	mio
)

GroupSourcesByFolder(PRTerrainLib)
