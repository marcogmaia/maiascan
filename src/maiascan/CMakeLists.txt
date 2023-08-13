
add_executable(maiascan "./main.cpp")

target_compile_definitions(maiascan PRIVATE "_AFXDLL")

target_link_libraries(maiascan PRIVATE cxxopts::cxxopts fmt::fmt)
