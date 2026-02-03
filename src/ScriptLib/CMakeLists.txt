file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(ScriptLib STATIC ${FILE_SOURCES})

target_link_libraries(ScriptLib
	mio
)

GroupSourcesByFolder(ScriptLib)
