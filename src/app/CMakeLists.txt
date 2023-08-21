add_executable(maiascan "./main_app.cpp" "./imgui_extensions.cpp")
target_link_libraries(maiascan PRIVATE imgui::imgui glfw scanner console
                                       glad::glad tl::optional)
target_compile_definitions(maiascan PRIVATE GLFW_INCLUDE_NONE)
