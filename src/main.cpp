#include <SFML/Graphics.hpp>
#include <cstdlib> 
#include <iostream>
#include <set>
#include <vector>

#define SHIP_SPEED 100
#define LASER_SPEED 150
#define MAX_LASERS 2
#define ALIEN_SHOOT_CHANCE 2

#define SCREEN_HEIGHT 256u
#define SCREEN_WIDTH 312u
#define PADDING 20.0f

#define MUS_PER_SEC 1000000


struct Laser {
    sf::Vector2f pos;
    sf::Vector2f size;
    sf::Vector2i sprite_index;
    int id;
};

struct Alien {
    sf::Vector2f pos;
    int alien_type;
    int id;
};

struct Alien_Type {
    sf::Vector2f size;
    sf::Vector2i sprite_index;
    sf::Vector2f shot_offset;
};

Alien_Type alien_type[3] = {
    {.size = {12.0f, 8.0f}, .sprite_index = {0,2}, .shot_offset = {5.0f,7.0f}},
    {.size = {10.0f, 9.0f}, .sprite_index = {1,2}, .shot_offset = {4.0f,7.0f}},
    {.size = {8.0f, 13.0f}, .sprite_index = {2,2}, .shot_offset = {3.0f,10.0f}},
};

struct Animation {
    sf::Vector2f pos;
    sf::Vector2i sprite_start;
    int current_frame;
    int frames;
    int mus_per_frame;
    int64_t mus_start;
    int id;
};


struct State {
    sf::RenderWindow window;
    sf::Texture sprites;
    sf::Sprite sprite;
    int id_counter;

    int level;

    struct {
        sf::Vector2f pos;
        sf::Vector2f size;
        bool dead;
        float respawn;
    } ship;

    int lives;

    struct {
        struct {
            bool hold, press;
        } left, right, shoot;
    } input;

    struct {
        float delta;
        sf::Int64 delta_mus;
        sf::Int64 now;
        sf::Int64 last_frame;
        sf::Int64 last_second;
        int fps;
        int frames;
    } time;

    std::vector<Laser> lasers;
    std::vector<Laser> alien_lasers;

    std::vector<Alien> aliens;
    enum MoveEnum {
        RIGHT,
        LEFT,
        DOWN,
    } last_move, move;

    std::vector<Animation> animations;
};

bool overlap(const sf::FloatRect a, const sf::FloatRect b) { return a.intersects(b); }

void new_level(State& state){
    for (int i = 0; i < state.level; i++) {
        for (int j = 0; j < 8; j++) {
            state.aliens.push_back(Alien{
                .pos ={20.0f + j * 16.0f, 20.0f + i * 16.0f,},
                .alien_type = i % 3,
                .id = state.id_counter++,
            });
        }
    }
}


void update(State& state) {
    // Ship
    auto ship_move = (state.input.right.hold - state.input.left.hold);
    state.ship.pos.x += SHIP_SPEED * ship_move * state.time.delta;

    if (state.ship.pos.x < PADDING) {
        state.ship.pos.x = PADDING;
    } else if ( state.ship.pos.x > SCREEN_WIDTH - state.ship.size.x - PADDING) {
        state.ship.pos.x = SCREEN_WIDTH - state.ship.size.x - PADDING;
    }

    if(state.ship.dead){
        state.ship.respawn -= state.time.delta;
        if(state.ship.respawn < 0 && state.lives > 0) {
            state.ship.pos = {
                (SCREEN_WIDTH-12)/ 2, 
                SCREEN_HEIGHT - state.ship.size.y - PADDING
            };
            state.ship.dead = false;
        }
    }


    // Lasers
    if (state.input.shoot.press) {
        if(state.lasers.size() < MAX_LASERS){
            state.lasers.push_back(Laser{
                .pos = state.ship.pos + sf::Vector2f{5.0f, 16.0f},
                .size = sf::Vector2f{2.0f, 7.0f},
                .sprite_index = sf::Vector2i{0,1},
                .id = state.id_counter++,
            });
            state.input.shoot.press = 0;
        }
    }

    std::erase_if(state.lasers, [](Laser& laser) { return laser.pos.y < -10; });

    for (auto& laser : state.lasers) {
        laser.pos.y -= LASER_SPEED * state.time.delta;
    }

    // Aliens

    // Update move dir
    if (state.move != State::DOWN) {
        state.last_move = state.move;
        bool change_dir = std::any_of(state.aliens.begin(), state.aliens.end(),
                                        [](Alien& a) { return a.pos.x < 20 || 
                                            a.pos.x > SCREEN_WIDTH - PADDING - alien_type[a.alien_type].size.x; 
                                        });
        if (change_dir) {
            state.move = State::MoveEnum::DOWN;
        }
    } else {
        state.move = state.last_move == State::MoveEnum::LEFT ? 
                                        State::MoveEnum::RIGHT : 
                                        State::MoveEnum::LEFT;
    }

    // move Aliens
    for (auto& alien : state.aliens) {
        switch (state.move) {
            case State::MoveEnum::RIGHT:
                alien.pos.x += 1;
                break;
            case State::MoveEnum::LEFT:
                alien.pos.x -= 1;
                break;
            case State::MoveEnum::DOWN:
                alien.pos.y += 10;
            default:
                break;
        }
    }

    // Aliens shoot
    for (const auto alien : state.aliens) {
        if(rand() % 1000 < ALIEN_SHOOT_CHANCE){
            state.alien_lasers.push_back(Laser{
                .pos = alien.pos + sf::Vector2f{5.0f,8.0f},
                .size = sf::Vector2f{2.0f,7.0f},
                .sprite_index = sf::Vector2i{0,4},
                .id = state.id_counter++,
            });
        }
    }

    std::erase_if(state.alien_lasers, [](Laser& laser) { return laser.pos.y > SCREEN_HEIGHT; });

    for (auto& laser : state.alien_lasers) {
        laser.pos.y += LASER_SPEED * state.time.delta;
    }

    // Check for collions
    std::set<int> ids_to_remove;

    for (const auto& laser : state.lasers) {
        auto laser_box = sf::FloatRect(laser.pos, laser.size);

        for (const auto& alien : state.aliens) {
            auto alien_box = sf::FloatRect(alien.pos, alien_type[alien.alien_type].size);

            if (overlap(laser_box, alien_box)) {
                state.animations.push_back(Animation {
                        .pos = alien.pos, 
                        .sprite_start{0, 3}, 
                        .current_frame = 0, 
                        .frames = 4, 
                        .mus_per_frame = 100000,
                        .mus_start = state.time.now, 
                        .id = state.id_counter++,
                    }
                );

                ids_to_remove.insert(laser.id);
                ids_to_remove.insert(alien.id);
            }
        }

        for (const auto& laser2 : state.alien_lasers) {
            const auto laser_box2 = sf::FloatRect(laser2.pos, laser2.size);
            if (overlap(laser_box, laser_box2))
            {
                ids_to_remove.insert(laser.id);
                ids_to_remove.insert(laser2.id);
            }
            
        }
        
    }

    const auto ship_box = sf::FloatRect(state.ship.pos, state.ship.size);
    for (const auto& laser : state.alien_lasers) {
        const auto laser_box = sf::FloatRect(laser.pos, laser.size);
        if (overlap(laser_box, ship_box)) {
            state.animations.push_back(Animation {
                .pos = state.ship.pos, 
                .sprite_start{0, 3}, 
                .current_frame = 0, 
                .frames = 4, 
                .mus_per_frame = 100000,
                .mus_start = state.time.now, 
                .id = state.id_counter++,
            });
            state.ship.pos = {-100,-100};
            state.ship.dead = true;
            state.lives--;
            state.ship.respawn = 1;
        }      
    }
    // Animations
    for (auto& ani : state.animations) {
        if (state.time.now - ani.mus_start > ani.mus_per_frame) {
            ani.current_frame++;
            if (ani.current_frame < ani.frames) {
                ani.mus_start = state.time.now;
            } else {
                ids_to_remove.insert(ani.id);
            }
        }
    }

    // Remove everything that should be.

    if (ids_to_remove.size() > 0) {
        std::erase_if(state.lasers, [ids_to_remove](Laser& laser) { return ids_to_remove.count(laser.id); });
        std::erase_if(state.alien_lasers, [ids_to_remove](Laser& laser) { return ids_to_remove.count(laser.id); });
        std::erase_if(state.aliens, [ids_to_remove](Alien& alien) { return ids_to_remove.count(alien.id); });
        std::erase_if(state.animations, [ids_to_remove](Animation& ani) { return ids_to_remove.count(ani.id); });
    }


    // New level

    if(state.aliens.size() < 1){
        state.level++;
        new_level(state);
    }
}

void draw_sprite(State& state, sf::Vector2i sprite_index, sf::Vector2f pos) {
    state.sprite.setTextureRect(sf::IntRect(sprite_index * 16, {16, 16}));

    state.sprite.setPosition(pos);

    state.window.draw(state.sprite);
}

void render(State& state) {
    state.window.clear();

    // Ship
    draw_sprite(state, {0, 0}, state.ship.pos);

    for (int i = 0; i < state.lives; i++){
        draw_sprite(state, {0,0},
            {4.0f + 16.0f * i, 4.0f}  
        );
    }
    

    // Lasers
    for (auto& laser : state.lasers) {
        draw_sprite(state, laser.sprite_index, laser.pos);
    }

    // Aliens
    for (auto& alien : state.aliens) {
        draw_sprite(state, alien_type[alien.alien_type].sprite_index, alien.pos);
    }
    for (auto& shot : state.alien_lasers) {
        draw_sprite(state, shot.sprite_index, shot.pos);
    }
    // Animations
    for (auto& ani : state.animations) {
        draw_sprite(state, ani.sprite_start + sf::Vector2i{ani.current_frame, 0}, ani.pos);
    }

    // Final draw
    state.window.display();
}

int main() {
    State state = {
        .window = sf::RenderWindow{{SCREEN_WIDTH, SCREEN_HEIGHT}, "Galactic_Assault"},
        .id_counter = 0,
        .level = 1,
        .ship{
            .pos = {(SCREEN_WIDTH-12)/ 2, SCREEN_HEIGHT - state.ship.size.y - PADDING},
            .size = {12,15},
        },
        .lives = 3,
        .move = State::MoveEnum::RIGHT,
    };

    state.window.setVerticalSyncEnabled(true);

    if (!state.sprites.loadFromFile("C:/Users/Mathias/dev/Galactic_Assault/sprites.png")) {
        std::cout << "Error when loading sprite: " << std::endl;
        return 1;
    }


    state.sprite.setTexture(state.sprites);

    new_level(state);

    sf::Clock clock;

    srand(clock.getElapsedTime().asMicroseconds());

    while (state.window.isOpen()) {
        // Handle
        sf::Int64 now = clock.getElapsedTime().asMicroseconds();

        state.time.now = now;
        state.time.delta_mus = now - state.time.last_frame;
        state.time.last_frame = now;
        state.time.delta = float(state.time.delta_mus) / MUS_PER_SEC;
        state.time.frames++;

        if (now - state.time.last_second > MUS_PER_SEC) {
            state.time.last_second = now;
            state.time.fps = state.time.frames;
            std::cout << "FPS: " << state.time.frames << "  Delta: " << state.time.delta_mus << "mus" << std::endl;
            state.time.frames = 0;
        }

        // Input
        for (auto event = sf::Event{}; state.window.pollEvent(event);) {
            if (event.type == sf::Event::Closed) {
                state.window.close();
            }
            if (event.type == sf::Event::KeyPressed) {
                switch (event.key.code) {
                    case sf::Keyboard::A:
                    case sf::Keyboard::Left: {
                        state.input.left.hold = true;
                        break;
                    }
                    case sf::Keyboard::D:
                    case sf::Keyboard::Right: {
                        state.input.right.hold = true;
                        break;
                    }
                    case sf::Keyboard::Space: {
                        state.input.shoot.press = !state.input.shoot.hold;
                        state.input.shoot.hold = true;
                        break;
                    }
                    case sf::Keyboard::Z: {
                        // Debug stop
                        break;
                    }
                }
            }
            if (event.type == sf::Event::KeyReleased) {
                switch (event.key.code) {
                    case sf::Keyboard::A:
                    case sf::Keyboard::Left: {
                        state.input.left.hold = false;
                        break;
                    }
                    case sf::Keyboard::D:
                    case sf::Keyboard::Right: {
                        state.input.right.hold = false;
                        break;
                    }
                    case sf::Keyboard::Space: {
                        state.input.shoot.hold = false;
                        state.input.shoot.press = false;
                        break;
                    }
                }
            }
        }

        now = clock.getElapsedTime().asMicroseconds();
        update(state);
        sf::Int64 update_time = clock.getElapsedTime().asMicroseconds() - now;

        now = clock.getElapsedTime().asMicroseconds();
        render(state);
        sf::Int64 render_time = clock.getElapsedTime().asMicroseconds() - now;
        if (state.time.frames == 0) {
            std::cout << "  Update: " << update_time << "mus" << std::endl;
            std::cout << "  Render: " << render_time << "mus" << std::endl;
        }
    }
}