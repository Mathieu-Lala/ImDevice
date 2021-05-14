#include <memory>
#include <entt/entt.hpp>
#include <SFML/Graphics.hpp>
#include <spdlog/spdlog.h>

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

struct Score {
    int component;
};

} // namespace component


namespace event {

struct Collide {
    entt::registry *world;
    // const component::RigidBody *other;
    entt::entity other;
};

struct UserInput {
    entt::registry *world;
    sf::Keyboard::Key key;
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

namespace object {

struct Ball {
    entt::entity entity;
    sf::CircleShape shape;

    Ball(entt::registry &world, float window_size_x, float window_size_y) : entity{world.create()}
    {
        constexpr auto radius = 10;
        shape = sf::CircleShape(radius);

        world.emplace<component::Drawable>(entity, &shape);
        world.emplace<component::Transformable>(entity, &shape);
        world.emplace<component::RigidBody>(entity, sf::FloatRect{0, 0, radius * 2, radius * 2});
        world.emplace<component::Position>(entity, sf::Vector2f{window_size_x / 2.0f, window_size_y / 2.0f});
        world.emplace<component::Velocity>(entity, sf::Vector2f{0.1f, 0.08f});

        world.ctx<entt::dispatcher *>()->sink<event::Collide>().connect<&Ball::on_collide>(*this);
    }

    auto on_collide(const event::Collide &event) -> void
    {
        if (event.world->has<entt::tag<"wall"_hs>>(event.other)) {
            event.world->patch<component::Velocity>(
                entity, [body = event.world->get<component::RigidBody>(event.other)](auto &vel) {
                    if (body.component.height >= body.component.width) {
                        vel.component.x *= -1.0f;
                    } else {
                        vel.component.y *= -1.0f;
                    }
                });
        } else {
            // todo : trigger increase score with a Game instance
            event.world->patch<component::Score>(event.other, [](auto &score) { score.component--; });
        }
    }
};

template<std::size_t ID>
struct Paddle {
    entt::entity entity;
    sf::RectangleShape shape;

    void on_move(const event::UserInput &event)
    {
        event.world->patch<component::Position>(entity, [&event](auto &pos) {
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

    Paddle(entt::registry &world, float window_size_x, float window_size_y) : entity{world.create()}
    {
        constexpr auto size_x = 10;
        constexpr auto size_y = 60;

        shape = sf::RectangleShape(sf::Vector2f{size_x, size_y});

        world.emplace<component::Drawable>(entity, &shape);
        world.emplace<component::Transformable>(entity, &shape);
        world.emplace<component::RigidBody>(entity, sf::FloatRect{0, 0, size_x, size_y});
        world.emplace<component::Position>(
            entity, sf::Vector2f{ID == 1 ? 0 : window_size_x - size_x, (window_size_y - size_y) / 2.0f});
        world.emplace<entt::tag<"paddle"_hs>>(entity);

        world.ctx<entt::dispatcher *>()->sink<event::UserInput>().connect<&Paddle::on_move>(*this);
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
            world.emplace<entt::tag<"wall"_hs>>(wall);
        }
    }
};

} // namespace object

} // namespace ecs

int main()
{
    constexpr auto window_size_x = 800;
    constexpr auto window_size_y = 800;

    sf::RenderWindow window{sf::VideoMode{window_size_x, window_size_y}, ""};

    entt::registry world{};
    entt::dispatcher dispatcher{};
    world.set<entt::dispatcher *>(&dispatcher);

    world.on_update<ecs::component::Position>().connect<&ecs::system::position_on_update>();
    world.on_construct<ecs::component::Position>().connect<&ecs::system::position_on_update>();

    ecs::object::Ball ball{world, window_size_x, window_size_y};
    ecs::object::Paddle<1> paddle1{world, window_size_x, window_size_y};
    ecs::object::Paddle<2> paddle2{world, window_size_x, window_size_y};
    ecs::object::Walls walls{world, window_size_x, window_size_y};

    sf::Clock clock{};
    while (window.isOpen()) {
        const auto elapsed_time = static_cast<double>(clock.restart().asMicroseconds()) / 1000.0;

        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) { window.close(); }
            if (event.type == sf::Event::KeyPressed) {
                dispatcher.trigger<ecs::event::UserInput>(&world, event.key.code);
            }
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
