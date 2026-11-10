// Self-check do ambiente interativo: fila de comidas e contagem de asteroides.
// Fica fora de src/ porque o Makefile compila todo .cpp de la (e aqui existe outro main).
#include <cassert>
#include <climits>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include "raylib.h"
#define private public  // o teste precisa contar as entidades do World
#include "world.hpp"
#undef private
#include "mode.hpp"

using namespace std;

static const int GOAL_A = 7, GOAL_B = 8;

static void test_food_queue(GameMode mode){
    current_mode = mode;
    Texture2D tex = {};
    Ship ship(800, 500, 1600, 1000, tex, 0, []{});
    ship.start_food_mode();

    assert(ship.getSensors()[GOAL_A] == 0 && ship.getSensors()[GOAL_B] == 0);

    ship.add_food({900, 500});
    ship.add_food({1500, 900});
    vector<double> first = ship.getSensors();
    assert(first[GOAL_A] != 0 || first[GOAL_B] != 0);

    // alcanca a primeira: o objetivo passa para a segunda, na ordem dos cliques
    ship.position = {900, 500};
    ship.update();
    assert(ship.current_goal_idx == 1);
    assert(ship.getSensors() != first);

    // alcanca a ultima: sem comida, sensores neutros
    ship.position = {1500, 900};
    ship.update();
    assert(ship.getSensors()[GOAL_A] == 0 && ship.getSensors()[GOAL_B] == 0);

    // nova comida depois da fila vazia volta a ser o objetivo
    ship.add_food({200, 200});
    assert(ship.getSensors()[GOAL_A] != 0 || ship.getSensors()[GOAL_B] != 0);

    // nunca fica matavel
    ship.coll_count = 10;
    ship.update();
    assert(!ship.killable);
}

static void test_asteroid_count(GameMode mode){
    current_mode = mode;
    vector<Texture2D> textures(3, Texture2D{});
    World world(1, 3, 1600, 1000, 32, textures, INT_MAX, {vector<double>(130, 0.5)}, false);
    world.start_food_mode();

    auto asteroids = [&]{
        return count_if(world.Entities.begin(), world.Entities.end(), [](Entity* e){ return e->type == 1; });
    };

    for(int count : {20, 1, 12, 2}){
        world.update();
        world.set_asteroid_count(count);
        world.update();  // computeFOV nao pode ler asteroide apagado
        assert(asteroids() == count);
        assert(world.food_ship->active);
    }

    // o jogador entra e sai na hora e, mesmo batendo muito, nao encerra o mundo
    auto players = [&]{
        return count_if(world.Entities.begin(), world.Entities.end(), [](Entity* e){ return e->type == 0; });
    };
    world.set_player(true);
    world.set_player(true);
    assert(players() == 1);
    Ship* player = dynamic_cast<Ship*>(world.Entities.back());
    player->coll_count = 10;
    world.update();
    assert(!player->killable);
    world.set_player(false);
    world.update();
    assert(players() == 0 && !world.isFinished());
}

int main(){
    for(GameMode mode : {GameMode::SharedGoals, GameMode::OwnGoal}){
        test_food_queue(mode);
        test_asteroid_count(mode);
    }
    cout << "ok" << endl;
    return 0;
}
