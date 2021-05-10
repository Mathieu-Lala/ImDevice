add_executable(app main.cpp)
target_link_libraries(app PRIVATE project_warnings project_options CONAN_PKG::imgui-sfml CONAN_PKG::spdlog
                                  CONAN_PKG::magic_enum)
# CONAN_PKG::entt CONAN_PKG::glm
