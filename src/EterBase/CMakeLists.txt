file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(EterBase STATIC ${FILE_SOURCES})

target_link_libraries(EterBase
	lzo2
	sodium
	mio
)

GroupSourcesByFolder(EterBase)
