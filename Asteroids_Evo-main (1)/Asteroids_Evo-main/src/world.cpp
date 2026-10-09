
#include "world.hpp"
#include "rng.hpp"
#include <cmath>
#include <memory>
#include <string>
#include <functional>

using namespace std;
    
// constructors & destructors

//make world from random genes
World::World(int botCount, int obstCount, int width, int height, float cell_size, vector<Texture2D>& textures, int duration, bool spawnplayer)
:Max_entities(botCount + obstCount + 1),
Entity_count(0),
World_width(width),
World_height(height),
Textures(textures),
Cell_size(cell_size),
grid(cell_size),
Duration(duration),
timer(0),
finished(false),
PLAYER_ACTIVE(spawnplayer)
{
    // spawn player
    if(PLAYER_ACTIVE) Spawn_entity(0);
    //Create the goal
    goal_points.push_back({ World_width * 0.05f, World_height * 0.95f });   // esquerda inferior
    goal_points.push_back({ World_width * 0.95f, World_height * 0.95f });   // direita inferior
    goal_points.push_back({ World_width * 0.95f, World_height * 0.05f });   // direita superior
    goal_points.push_back({ World_width * 0.05f, World_height * 0.05f });   // esquerda superior
    has_goal = true;
    // spawn obstacles
    for(int i = 0; i < obstCount; i++) Spawn_entity(1);

    // spawn bots with random genes
    alive = botCount;

    for(int i = 0; i < botCount; i++) {
        Ship* ship = dynamic_cast<Ship*>(Spawn_entity(2)); //create the ship
        vector<double> random_genome;
            for(int j = 0; j < 40; j++){  // 40 = GENOME_SIZE (main.cpp)
                random_genome.push_back(get_random_double(-1,1));
            }
        if(has_goal) ship->set_goals(goal_points);
        population.emplace_back(ship, random_genome); //put the bot in the ship
    }
}

//make world from list of genes
World::World(int botCount, int obstCount, int width, int height, float cell_size, vector<Texture2D>& textures, int duration, vector<vector<double>> genomes, bool spawnplayer)
:Max_entities(botCount + obstCount + 1),
Entity_count(0),
World_width(width),
World_height(height),
Textures(textures),
Cell_size(cell_size),
grid(cell_size),
Duration(duration),
timer(0),
finished(false),
PLAYER_ACTIVE(spawnplayer)
{
    // spawn player
    if(PLAYER_ACTIVE) Spawn_entity(0);
    goal_points.clear();
    goal_points.push_back({ World_width * 0.05f, World_height * 0.95f });   // esquerda inferior
    goal_points.push_back({ World_width * 0.95f, World_height * 0.95f });   // direita inferior
    goal_points.push_back({ World_width * 0.95f, World_height * 0.05f });   // direita superior
    goal_points.push_back({ World_width * 0.05f, World_height * 0.05f });   // esquerda superior
    has_goal = true;


    // spawn obstacles
    for(int i = 0; i < obstCount; i++) Spawn_entity(1);

    // spawn bots with desired genes
    alive = botCount;

    for(int i = 0; i < botCount; i++) {
                Ship* ship = dynamic_cast<Ship*>(Spawn_entity(2));;
                if(i == 0) ship->set_best_ship();
                if(has_goal) ship->set_goals(goal_points);
                population.emplace_back(ship, genomes[i]);
    }
}

World::~World(){
    for(auto e : Entities){
        delete e;
    }
};


//getters & setters -------------------------------------------------------------------------

int World::getTime(){
    return timer;
}

int World::getAlive(){
    return alive;
}

void World::setTime(int newTime){
    timer = newTime;
}

bool World::isFinished(){
    return finished;
}

vector<pair<vector<double>,float>> World::getResult(){
    vector<pair<vector<double>,float>> result(population.size());

    for(int i = 0; i < population.size(); i++){
        result[i].first = population[i].get_genome();
        result[i].second = population[i].get_score();
    }

    return result;
}

// methods ----------------------------------------------------------------------------------

//spawn a new entity. types: 0- player ship, 1- asteroid, 2- bot ship
Entity* World::Spawn_entity(int type){
    Entity* new_entity;
    std::function<void(void)> death_callback = [&](){alive--;};

    switch(type){
        case 0:
            new_entity = new Ship(World_width/2, World_height/2, World_width, World_height, Textures[type], Entity_count++, death_callback);
        break;

        case 1:
            new_entity = new Asteroid(World_width/2,World_height/2,World_width,World_height, Textures[type], Entity_count++, death_callback);
        break;

        case 2:
            new_entity = new Ship(World_width/2,World_height/2,World_width,World_height, Textures[type], Entity_count++, death_callback);
            new_entity->collisionradius = 20;
            new_entity->type = 2;
        break;
    }

    while(check_valid_spawn(new_entity) == false){
        new_entity->randomize_position();
    }

    Entities.push_back(new_entity);
    return new_entity;
}

//update position, check for and respond to collision for all entities
//also update desired movement for all bots
void World::update(){
    if(isFinished()) return;
    if(timer >= Duration || Entities.empty() || alive == 0){
        finished = true;
        return;
    }

    timer++;

    grid.computeFOV(Entities); 
    
    for(auto& bot : population) {
        bot.movement();
    }

    for(auto& e : Entities){
        e->update();
    }

    //check collision between all entities (With Hash map)
    grid.build(Entities);
    grid.computeCollisions(Entities);
}

//checks if an entity isn't colliding with another
bool World::check_valid_spawn(Entity* candidate){

    for(size_t i = 0; i < Entities.size(); ++i){
        if (Entities[i]->active == false) continue;
        if (candidate->coll_check(Entities[i])) return false;
    }
    return true;

}

//draw all entities
void World::Draw(){
    for(auto e: Entities) e->Draw();
}

//drawextra all entities
void World::DrawExtra(){
    for(auto e: Entities) e->DrawExtra();
    if(has_goal){
        for(int i = 0; i < goal_points.size(); i++){
            Color c;
            if(i == 0) c = GREEN;          // primeiro objetivo
            else if(i == goal_points.size()-1) c = GOLD;  // último
            else c = ORANGE;               // intermediários
            
            DrawCircleV(goal_points[i], 50, { c.r, c.g, c.b, 80 });
            DrawCircleLines(goal_points[i].x, goal_points[i].y, 50, c);
            DrawCircleV(goal_points[i], 10, c);
            
            // numera cada objetivo
            DrawText(TextFormat("%d", i+1), goal_points[i].x - 5, goal_points[i].y - 60, 15, c);
        }
    }
    
}
