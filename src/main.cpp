#include <memory>
#include <entt/entt.hpp>
#include <SFML/Graphics.hpp>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace ecs {

namespace component {

struct Drawable {
    sf::Drawable *component;
};

struct Transformable {
    sf::Transformable *component;
};

struct Velocity {
    sf::Vector2f component;
};

struct RigidBody {
    sf::FloatRect component;
};

struct Position {
    sf::Vector2f component;
};

// struct Score {
//     int component;
// };

struct Clock {
    std::chrono::milliseconds milliseconds_target;
    std::chrono::milliseconds milliseconds_current;
};

struct ClockIsExecutable {
};

} // namespace component

namespace event {

struct Collide {
    entt::registry *world;
    entt::entity other;
};

struct UserInput {
    entt::registry *world;
    sf::Keyboard::Key key;
};

struct ClockExecuted {
    entt::registry *world;
    entt::entity object;
};

} // namespace event

namespace system {

auto position_on_update(entt::registry &world, entt::entity entity)
{
    const auto position = world.get<component::Position>(entity);

    const auto transformable = world.try_get<component::Transformable>(entity);
    if (transformable != nullptr) { transformable->component->setPosition(position.component); }

    const auto rigid_body = world.try_get<component::RigidBody>(entity);
    if (rigid_body != nullptr) {
        rigid_body->component.left = position.component.x;
        rigid_body->component.top = position.component.y;

        for (const auto &i : world.view<component::Position, component::RigidBody>()) {
            if (i == entity) continue;

            const auto &other_rigid_body = world.get<component::RigidBody>(i);

            if (other_rigid_body.component.intersects(rigid_body->component)) {
                world.ctx<entt::dispatcher *>()->trigger<event::Collide>(&world, i);
                break;
            }
        }
    }
}

} // namespace system

} // namespace ecs

namespace game {

namespace event {

struct WallHit {
    entt::registry *world;
};

} // namespace event

namespace object {

struct Ball {
    entt::entity entity;
    sf::CircleShape shape;

    constexpr static float radius = 10;

    Ball(entt::registry &world, float window_size_x, float window_size_y) :
        entity{world.create()}, shape{radius}
    {
        world.emplace<ecs::component::Drawable>(entity, &shape);
        world.emplace<ecs::component::Transformable>(entity, &shape);
        world.emplace<ecs::component::RigidBody>(entity, sf::FloatRect{0, 0, radius * 2, radius * 2});
        world.emplace<ecs::component::Clock>(entity, 2000ms, std::chrono::milliseconds::zero());

        reset(world, window_size_x, window_size_y);

        world.ctx<entt::dispatcher *>()->sink<ecs::event::Collide>().connect<&Ball::on_collide>(*this);
        world.ctx<entt::dispatcher *>()->sink<ecs::event::ClockExecuted>().connect<&Ball::on_clock_execute>(*this);
    }

    auto reset(entt::registry &world, float window_size_x, float window_size_y) -> void
    {
        world.emplace_or_replace<ecs::component::Position>(
            entity, sf::Vector2f{window_size_x / 2.0f, window_size_y / 2.0f});

        const auto vec = sf::Vector2f{
            ((std::rand() & 1) ? 1.0f : -1.0f) * 0.01f,
            ((std::rand() & 1) ? 1.0f : -1.0f) * static_cast<float>(std::rand() % 9 + 1) / 1000.0f};

        spdlog::info("{}, {}", vec.x, vec.y);


        world.emplace_or_replace<ecs::component::Velocity>(entity, vec);
    }

    auto on_clock_execute(const ecs::event::ClockExecuted &event) -> void
    {
        if (event.object != entity) return;

        // spdlog::info("clock executed");

        event.world->patch<ecs::component::Velocity>(entity, [](auto &vel) {
            vel.component.x *= 1.01f;
            vel.component.y *= 1.01f;
        });
    }

    auto on_collide(const ecs::event::Collide &event) -> void
    {
        if (event.world->has<entt::tag<"trigger_wall_hit"_hs>>(event.other)) {
            event.world->ctx<entt::dispatcher *>()->trigger<event::WallHit>(event.world);

        } else {
            event.world->patch<ecs::component::Velocity>(
                entity, [body = event.world->get<ecs::component::RigidBody>(event.other)](auto &vel) {
                    if (body.component.height >= body.component.width) {
                        vel.component.x *= -1.0f;
                    } else {
                        vel.component.y *= -1.0f;
                    }
                });
        }
    }
};

template<std::size_t ID>
struct Paddle {
    entt::entity entity;
    sf::RectangleShape shape;

    void on_move(const ecs::event::UserInput &event)
    {
        event.world->patch<ecs::component::Position>(entity, [&event](auto &pos) {
            constexpr auto speed = 5;
            if constexpr (ID == 1) {
                if (event.key == sf::Keyboard::R) {
                    pos.component.y -= speed;
                } else if (event.key == sf::Keyboard::F) {
                    pos.component.y += speed;
                }
            } else {
                if (event.key == sf::Keyboard::I) {
                    pos.component.y -= speed;
                } else if (event.key == sf::Keyboard::K) {
                    pos.component.y += speed;
                }
            }
        });
    }

    constexpr static float size_x = 10;
    constexpr static float size_y = 60;

    Paddle(entt::registry &world, float window_size_x, float window_size_y) :
        entity{world.create()}, shape{sf::Vector2f{size_x, size_y}}
    {
        world.emplace<ecs::component::Drawable>(entity, &shape);
        world.emplace<ecs::component::Transformable>(entity, &shape);
        world.emplace<ecs::component::RigidBody>(entity, sf::FloatRect{0, 0, size_x, size_y});
        world.emplace<ecs::component::Position>(
            entity, sf::Vector2f{ID == 1 ? 0 : window_size_x - size_x, (window_size_y - size_y) / 2.0f});

        world.ctx<entt::dispatcher *>()->sink<ecs::event::UserInput>().connect<&Paddle::on_move>(*this);
    }
};

struct Walls {
    Walls(entt::registry &world, float window_size_x, float window_size_y)
    {
        struct S {
            ecs::component::Position pos;
            ecs::component::RigidBody body;
        };
        constexpr auto size = 10;
        auto walls = std::to_array({
            S{{{0, -size}}, {{0, 0, window_size_x, size}}},
            S{{{window_size_x, 0}}, {{0, 0, size, window_size_y}}},
            S{{{0, window_size_y}}, {{0, 0, window_size_x, size}}},
            S{{{-size, 0}}, {{0, 0, size, window_size_y}}},
        });
        for (std::size_t i = 0; i != 4; i++) {
            const auto wall = world.create();
            world.emplace<ecs::component::RigidBody>(wall, walls[i].body);
            world.emplace<ecs::component::Position>(wall, walls[i].pos);
            if (i & 1) { world.emplace<entt::tag<"trigger_wall_hit"_hs>>(wall); }
        }
    }
};

} // namespace object

struct Pong {
    float window_size_x;
    float window_size_y;
    object::Ball ball;
    object::Paddle<1> paddle1;
    object::Paddle<2> paddle2;
    object::Walls walls;

    Pong(entt::registry &world, float size_x, float size_y) :
        window_size_x{size_x}, window_size_y{size_y}, ball{world, window_size_x, window_size_y},
        paddle1{world, window_size_x, window_size_y}, paddle2{world, window_size_x, window_size_y},
        walls{world, window_size_x, window_size_y}
    {
        world.ctx<entt::dispatcher *>()->sink<event::WallHit>().connect<&Pong::on_wall_hit>(*this);
    }

    void on_wall_hit(const event::WallHit &event) { ball.reset(*event.world, window_size_x, window_size_y); }
};

} // namespace game

int main()
{
    std::srand(static_cast<std::uint32_t>(std::time(nullptr)));

    constexpr auto window_size_x = 800;
    constexpr auto window_size_y = 800;

    sf::RenderWindow window{sf::VideoMode{window_size_x, window_size_y}, ""};

    entt::registry world{};
    entt::dispatcher dispatcher{};
    world.set<entt::dispatcher *>(&dispatcher);
    world.on_update<ecs::component::Position>().connect<&ecs::system::position_on_update>();
    world.on_construct<ecs::component::Position>().connect<&ecs::system::position_on_update>();

    game::Pong pong{world, window_size_x, window_size_y};

    sf::Clock clock{};
    while (window.isOpen()) {
        const auto elapsed_time = static_cast<double>(clock.restart().asMicroseconds()) / 100.0;

        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) { window.close(); }
            if (event.type == sf::Event::KeyPressed) {
                dispatcher.trigger<ecs::event::UserInput>(&world, event.key.code);
            }
        }

        for (const auto &i : world.view<ecs::component::Clock>()) {
            auto &clock_component = world.get<ecs::component::Clock>(i);
            clock_component.milliseconds_current +=
                std::chrono::milliseconds{static_cast<std::int64_t>(elapsed_time)};
            // spdlog::info("{}", elapsed_time);
            // spdlog::info("{}", clock_component.milliseconds_current.count());
            if (clock_component.milliseconds_current >= clock_component.milliseconds_target) {
                clock_component.milliseconds_current = std::chrono::milliseconds::zero();
                world.emplace<ecs::component::ClockIsExecutable>(i);
            }
        }

        for (const auto &i : world.view<ecs::component::ClockIsExecutable>()) {
            world.remove<ecs::component::ClockIsExecutable>(i);
            dispatcher.trigger<ecs::event::ClockExecuted>(&world, i);
        }

        for (const auto &e : world.view<ecs::component::Position, ecs::component::Velocity>()) {
            const auto &vel = world.get<ecs::component::Velocity>(e);
            world.patch<ecs::component::Position>(e, [&vel, &elapsed_time](auto &pos) {
                pos.component += vel.component * static_cast<float>(elapsed_time);
            });
        }

        // world.view<ecs::component::Position>().each([&window](const auto &pos) {
        //     spdlog::info("{{.x: {}, .y: {}}}", pos.component.x, pos.component.y);
        // });

        window.clear();
        world.view<ecs::component::Drawable>().each(
            [&window](const auto &drawable) { window.draw(*drawable.component); });
        window.display();
    }
}
