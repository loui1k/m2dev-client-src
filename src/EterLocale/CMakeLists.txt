file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(EterLocale STATIC ${FILE_SOURCES})

target_link_libraries(EterLocale
	mio
)

GroupSourcesByFolder(EterLocale)
