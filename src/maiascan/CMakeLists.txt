add_subdirectory(console)
add_subdirectory(scanner)

add_executable(maiascan "./main.cpp")

target_compile_definitions(maiascan PRIVATE "_AFXDLL")

target_link_libraries(maiascan PRIVATE scanner cxxopts::cxxopts fmt::fmt)
