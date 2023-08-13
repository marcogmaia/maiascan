
add_library(scanner "./scanner.cpp")

target_compile_definitions(scanner PRIVATE "_AFXDLL")

target_link_libraries(scanner PRIVATE fmt::fmt)
