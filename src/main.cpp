#include <optional>
#include <chrono>

#include <imgui-SFML.h>
#include <imgui.h>

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/System/Clock.hpp>

#include <spdlog/spdlog.h>

#include <magic_enum.hpp>

using namespace std::chrono_literals;

struct MouseState {
    enum Button {
        Left,
        Right,
        Middle,
        XButton1,
        XButton2,

        ButtonCount
    };

    enum WheelState { UP, DOWN };

    bool is_in_window;
    sf::Vector2i pos;

    std::array<bool, Button::ButtonCount> button_state;

    std::optional<WheelState> wheel_state;

    auto set_wheel_state(float delta)
    {
        if (delta < 0) {
            wheel_state = MouseState::WheelState::DOWN;
        } else if (delta > 0) {
            wheel_state = MouseState::WheelState::UP;
        }

        m_time_point_since_wheel_changed = std::chrono::system_clock::now();
    }

    auto tick()
    {
        const auto now = std::chrono::system_clock::now();

        if (m_time_point_since_wheel_changed.time_since_epoch() + 100ms <= now.time_since_epoch()) {
            wheel_state = {};
        }
    }

private:
    using timestamp = decltype(std::chrono::system_clock::now());
    timestamp m_time_point_since_wheel_changed;
};

template<typename... Args>
void text(const std::string_view fmt, Args &&... args)
{
    ImGui::Text("%s", fmt::format(fmt, args...).data());
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(1080, 760), "ImDevice");
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    sf::Texture texture;
    if (!texture.loadFromFile("./asset/mouse-4/empty.png")) { return 1; }

    MouseState mouse_state;

    sf::Clock delta_clock;
    while (window.isOpen()) {
        // mouse_state.wheel_state = {};

        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) { window.close(); }

            if (event.type == sf::Event::MouseButtonPressed) {
                mouse_state.button_state[event.mouseButton.button] = true;
            }
            if (event.type == sf::Event::MouseButtonReleased) {
                mouse_state.button_state[event.mouseButton.button] = false;
            }
            if (event.type == sf::Event::MouseEntered) { mouse_state.is_in_window = true; }
            if (event.type == sf::Event::MouseLeft) { mouse_state.is_in_window = false; }
            if (event.type == sf::Event::MouseMoved) {
                mouse_state.pos = {event.mouseMove.x, event.mouseMove.y};
            }
            if (event.type == sf::Event::MouseWheelScrolled) {
                mouse_state.set_wheel_state(event.mouseWheelScroll.delta);
            }
        }

        mouse_state.tick();
        ImGui::SFML::Update(window, delta_clock.restart());

        ImGui::Begin("Device Mouse");
        if (!mouse_state.is_in_window) {
            ImGui::Text("No mouse in the window");
        } else {
            text("position: (x: {}, y: {})", mouse_state.pos.x, mouse_state.pos.y);

            for (auto i = 0u; i != MouseState::Button::ButtonCount; i++) {
                const auto button = magic_enum::enum_cast<MouseState::Button>(i).value();
                text("button::{}: {}", magic_enum::enum_name(button).data(), mouse_state.button_state[button]);
            }
            text(
                "wheel: {}",
                mouse_state.wheel_state.has_value()
                    ? magic_enum::enum_name(mouse_state.wheel_state.value()).data()
                    : "none");

            ImGui::Image(texture);
        }
        ImGui::End();

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();

    return 0;
}
