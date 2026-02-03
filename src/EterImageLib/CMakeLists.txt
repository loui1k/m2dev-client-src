file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(EterImageLib STATIC ${FILE_SOURCES})

target_link_libraries(EterImageLib
	mio
)

GroupSourcesByFolder(EterImageLib)
