file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(EterGrnLib STATIC ${FILE_SOURCES})

target_link_libraries(EterGrnLib
	mio
)

GroupSourcesByFolder(EterGrnLib)
