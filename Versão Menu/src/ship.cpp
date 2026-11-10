#include "ship.hpp"
#include "mode.hpp"
#include <raymath.h>
#include <cmath>
#include <functional>

using namespace std;

enum class SensorType {
    Speed,
    Alignment,
    
    // Sensores de Proximidade (Raycasts)
    Ray_Left90,   // Esquerda total
    Ray_Left45,   // Diagonal Esquerda
    Ray_Front,    // Frente
    Ray_Right45,  // Diagonal Direita
    Ray_Right90,  // Direita total
    
    // Sensores de Objetivo (Goal) - o significado depende do modo
    // SharedGoals: GoalA = direção até o objetivo (-1 esquerda, +1 direita), GoalB = alinhamento (1 de frente, -1 de costas)
    // OwnGoal:     GoalA = distância normalizada até o objetivo,            GoalB = direção até o objetivo
    GoalA,
    GoalB,

    COUNT
};

// constructor -----------------------------------------------------------------------------
Ship::Ship(float x, float y, int window_w, int window_h, Texture2D& ship_tex, unsigned int id, std::function<void(void)> callback)
: Entity(x,y,window_w,window_h,ship_tex,id,callback)
{
    acceleration           = 0.4;
    angular_acceleration   = 0.01;
    angular_drag           = 0.95;
    max_angular_velocity   = 0.1;
    collisionradius        = 25;
    drag                   = 0.98;
    type                   = 0;
    score                  = 0;
    desired_movement_accumulator = 0;
    desired_movement_accumulator2 = 0;
    ray_max_dist           = 150;
    goal_generation_radius = 300.0f;
    goal_reach_radius      = 50.0f;
    goal_reward            = 5000.0f;
    current_goal           = {0, 0};
    killable               = false;
    is_best                = false;
    food_mode              = false;
    has_goal = false;
    current_goal_idx = 0;
    goals.clear();

    ray_sensors.resize(NUM_RAYS, 0.0f);
    
}


// methods ----------------------------------------------------------------------------------
vector<bool> Ship::scan_inputs(){
    vector<bool> output(4);
    output = {false,false,false,false};

    if(IsKeyDown(KEY_DOWN)){
        output[0] = true;
    }
    if(IsKeyDown(KEY_UP)){
        output[1] = true;
    }
    if(IsKeyDown(KEY_RIGHT)){
        output[2] = true;
    }
    if(IsKeyDown(KEY_LEFT)){
        output[3] = true;
    }

    return(output);
}

void Ship::movement(vector<bool> inputs){
    // rotate right
    if (inputs[2]) angularvelocity += angular_acceleration;
    
    // rotate left
    if (inputs[3]) angularvelocity -= angular_acceleration;
    
    // thrust forward
    if (inputs[1]){
        speeds.x += acceleration*cosf(facing_angle);
        speeds.y += acceleration*sinf(facing_angle);
    }
    
    // thrust backward
    if (inputs[0]){
        speeds.x -= 0.5*acceleration*cosf(facing_angle);
        speeds.y -= 0.5*acceleration*sinf(facing_angle);
    }
}
void Ship::set_goals(std::vector<Vector2> new_goals){
    goals = new_goals;
    has_goal = true;
    current_goal_idx = 0;
}

void Ship::start_food_mode(){
    food_mode = true;
    has_goal = false;
    goals.clear();
    current_goal_idx = 0;
}

void Ship::add_food(Vector2 food){
    goals.push_back(food);
    // a fila estava vazia: a nova comida ja e o objetivo atual
    if(current_goal_idx == (int)goals.size() - 1){
        current_goal = food;
        has_goal = true;
    }
}

void Ship::draw_food(){
    for(int i = current_goal_idx; i < (int)goals.size(); i++) DrawCircleV(goals[i], 12, GOLD);
}

// sorteia um objetivo na borda de um raio ao redor da nave (GameMode::OwnGoal)
void Ship::generate_goal(){
    const float margin = 50.0f;
    const float radius = goal_generation_radius;

    for(int attempt = 0; attempt < 200; attempt++){
        float angle = GetRandomValue(0, 359) * DEG2RAD;

        Vector2 candidate = {
            position.x + cosf(angle) * radius,
            position.y + sinf(angle) * radius
        };

        if(candidate.x >= margin &&
           candidate.x <= screenWidth - margin &&
           candidate.y >= margin &&
           candidate.y <= screenHeight - margin)
        {
            current_goal = candidate;
            has_goal = true;
            return;
        }
    }

    current_goal = {
        Clamp(position.x, margin, screenWidth - margin),
        Clamp(position.y, margin, screenHeight - margin)
    };

    has_goal = true;
}

Vector2 Ship::get_goal() const{
    return current_goal;
}

float Ship::get_goal_distance() const{
    if(!has_goal) return 0.0f;

    return Vector2Distance(position, current_goal);
}

void Ship::update()
{   
    if(!active) return;
    Entity::update();
    
    if(coll_count > 4 && killable == false && !food_mode){
        killable = true;
    }

    if(type == 0) movement(scan_inputs());
    angularvelocity *= angular_drag;
    speeds = { speeds.x*(float)drag, speeds.y*(float)drag };

    //cap ship angular velocity
    if(angularvelocity > max_angular_velocity) angularvelocity = max_angular_velocity;
    if(angularvelocity < -max_angular_velocity) angularvelocity = -max_angular_velocity;

    desired_movement_accumulator += abs_speed*alignment_coefficient;
    if(current_mode == GameMode::SharedGoals){
        if(has_goal && current_goal_idx < goals.size()){
            float current_dist = Vector2Distance(position, goals[current_goal_idx]);
            if(current_dist < 50.0f){
                desired_movement_accumulator2 += 10000.0;
                current_goal_idx++;
            }
        }
    }
    else if(has_goal){
        float current_dist = Vector2Distance(position, current_goal);
        if(current_dist <= goal_reach_radius){
            desired_movement_accumulator2 += goal_reward * alignment_coefficient;
            if(food_mode){
                current_goal_idx++;
                has_goal = current_goal_idx < (int)goals.size();
                if(has_goal) current_goal = goals[current_goal_idx];
            }
            else generate_goal();
        }
    }
    score = desired_movement_accumulator*(1-0.15*coll_count) + desired_movement_accumulator2;
    if(score < 0) score = 0;

}

void Ship::set_best_ship(){
    is_best = true;
}

void Ship::reset_sensors() {
    std::fill(ray_sensors.begin(), ray_sensors.end(), 0.0f);
}

void Ship::sense_walls() {
    if (!active) return;
    float angles[] = { -90, -45, 0, 45, 90 };
    
    // Loop pelos 5 sensores
    for(int i = 0; i < NUM_RAYS; i++) {
        float ang_rad = facing_angle + angles[i] * DEG2RAD;
        
        // Vetor direção do raio
        float dx = cosf(ang_rad);
        float dy = sinf(ang_rad);

        // Evita divisão por zero
        if (abs(dx) < 0.0001f) dx = 0.0001f;
        if (abs(dy) < 0.0001f) dy = 0.0001f;

        float dist_wall = ray_max_dist * 2.0f; // Começa com valor alto (longe)

        // --- Matemática de Intersecção Ray-Box (AABB) ---
        
        // 1. Checa paredes Verticais (X)
        if (dx > 0) {
            // Raio indo para direita -> Distância até screenWidth
            float d = (screenWidth - position.x) / dx;
            if (d < dist_wall) dist_wall = d;
        } else {
            // Raio indo para esquerda -> Distância até 0
            float d = (0 - position.x) / dx;
            if (d < dist_wall) dist_wall = d;
        }

        // 2. Checa paredes Horizontais (Y)
        if (dy > 0) {
            // Raio indo para baixo -> Distância até screenHeight
            float d = (screenHeight - position.y) / dy;
            if (d < dist_wall) dist_wall = d;
        } else {
            // Raio indo para cima -> Distância até 0
            float d = (0 - position.y) / dy;
            if (d < dist_wall) dist_wall = d;
        }

        // --- Processa o sinal do sensor ---
        
        // Se a parede está dentro do alcance da visão
        if (dist_wall < ray_max_dist) {
            // Inverte para input neural (1.0 = muito perto, 0.0 = longe/sem nada)
            float signal = 1.0f - (dist_wall / ray_max_dist);
            
            // Clamping
            if (signal < 0) signal = 0;
            if (signal > 1) signal = 1;

            // O sensor pega o MAIOR perigo (seja parede ou asteroide que já foi calculado)
            if (signal > ray_sensors[i]) {
                ray_sensors[i] = signal;
            }
        }
    }
}

void Ship::update_sensor_with_entity(Entity* other) {
    if (!active || !other->active) return;

    Vector2 to_obj = { other->get_position().x - position.x, other->get_position().y - position.y };
    float dist_sq = to_obj.x * to_obj.x + to_obj.y * to_obj.y;
    
    // if the object's closest possible edge is beyond our max ray distance, skip it
    float max_reach = ray_max_dist + other->get_collRadius();
    if (dist_sq > max_reach * max_reach) return;

    float sensor_angles_deg[5] = {-90.0f, -45.0f, 0.0f, 45.0f, 90.0f};
    
    // Check intersection for each of the 5 sensor rays
    for (int i = 0; i < 5; ++i) {
        // 1. Calculate the actual world angle and direction of this specific ray
        float ray_angle = facing_angle + (sensor_angles_deg[i] * (PI / 180.0f));
        Vector2 ray_dir = { cosf(ray_angle), sinf(ray_angle) };
        
        // 2. Project the vector pointing to the object onto the ray's direction vector
        // This finds the closest point on the ray's infinite line to the object's center
        float tca = (to_obj.x * ray_dir.x) + (to_obj.y * ray_dir.y);
        
        // 3. If tca is negative, the object's center is behind the ship.
        // We only care if we are somehow spawned *inside* the object's radius.
        if (tca < 0.0f && dist_sq > (other->get_collRadius() * other->get_collRadius())) {
            continue; 
        }
        
        // 4. Calculate the perpendicular distance squared from the object's center to the ray
        float d2 = dist_sq - (tca * tca);
        float radius_sq = other->get_collRadius() * other->get_collRadius();
        
        // 5. If that distance is greater than the radius, the ray misses the circle completely
        if (d2 > radius_sq) {
            continue; 
        }
        
        // 6. The ray hits! Calculate the exact distance to the circle's surface
        float thc = sqrtf(radius_sq - d2); // Distance from the projected center to the edge
        float dist_to_surface = tca - thc; 
        
        // If we are currently inside the object, distance to surface is 0
        if (dist_to_surface < 0.0f) {
            dist_to_surface = 0.0f;
        }

        // If the surface is further than the sensor can see, ignore it
        if (dist_to_surface > ray_max_dist) {
            continue;
        }

        // 7. Calculate the signal strength (1.0 = touching, 0.0 = at max distance)
        float signal = 1.0f - (dist_to_surface / ray_max_dist);
        
        if (signal < 0.0f) signal = 0.0f;
        if (signal > 1.0f) signal = 1.0f;

        // 8. Apply the signal if it's the strongest one we've seen so far on this sensor
        if (signal > ray_sensors[i]) {
            ray_sensors[i] = signal;
        }
    }
}

vector<double> Ship::getSensors() const {
    vector<double> s((size_t)SensorType::COUNT);

    s[(int)SensorType::Speed]  = abs_speed/20.0f + 0.01;       // Speed divided by 20.0 for normalization
    s[(int)SensorType::Alignment] = alignment_coefficient;

    s[(int)SensorType::Ray_Left90]  = ray_sensors[0];
    s[(int)SensorType::Ray_Left45]  = ray_sensors[1];
    s[(int)SensorType::Ray_Front]   = ray_sensors[2];
    s[(int)SensorType::Ray_Right45] = ray_sensors[3];
    s[(int)SensorType::Ray_Right90] = ray_sensors[4];

    // --- Sensores de Objetivo ---
    if (current_mode == GameMode::OwnGoal) {
        if (has_goal) {
            Vector2 to_goal = { current_goal.x - position.x, current_goal.y - position.y };

            float normalized_distance = Vector2Length(to_goal) / goal_generation_radius;
            if (normalized_distance > 1.0f) normalized_distance = 1.0f;

            float relative_angle = atan2f(to_goal.y, to_goal.x) - facing_angle;
            while (relative_angle >  PI) relative_angle -= 2.0f * PI;
            while (relative_angle < -PI) relative_angle += 2.0f * PI;

            s[(int)SensorType::GoalA] = normalized_distance;
            s[(int)SensorType::GoalB] = relative_angle / PI;
        } else {
            s[(int)SensorType::GoalA] = 0.0;
            s[(int)SensorType::GoalB] = 0.0;
        }
        return s;
    }

    // Calculam a direção até o objetivo atual relativa à proa da nave
    if (has_goal && current_goal_idx < goals.size()) {
        float dx = goals[current_goal_idx].x - position.x;
        float dy = goals[current_goal_idx].y - position.y;

        float angle_to_goal = atan2f(dy, dx);

        // diferença angular entre "para onde a nave aponta" e "para onde está o objetivo"
        float diff = angle_to_goal - facing_angle;

        // normaliza para o intervalo [-PI, PI]
        while (diff >  PI) diff -= 2.0f * PI;
        while (diff < -PI) diff += 2.0f * PI;

        s[(int)SensorType::GoalA] = diff / (float)PI;  // -1 (esquerda) .. +1 (direita)
        s[(int)SensorType::GoalB] = cosf(diff);        // 1 = na direção certa, -1 = de costas
    } else {
        // sem objetivo ativo: sensores neutros
        s[(int)SensorType::GoalA] = 0.0;
        s[(int)SensorType::GoalB] = 0.0;
    }

    return s;
}


void Ship::DrawExtra(){
    if(!active) return;

    Entity::DrawExtra();
    if(current_mode == GameMode::OwnGoal) {
        if(has_goal) {
            float dist = Vector2Distance(position, current_goal);
            DrawCircleV(current_goal, goal_reach_radius, {255, 255, 0, 60});
            DrawCircleLines(current_goal.x, current_goal.y, goal_reach_radius, YELLOW);
            DrawCircleV(current_goal, 8, YELLOW);
            DrawLineV(position, current_goal, YELLOW);
            DrawText(TextFormat("Goal dist: %.0f", dist), position.x + 20, position.y + 60, 10, YELLOW);
        }
    }
    else if(has_goal && current_goal_idx < goals.size()) {
        float dist = Vector2Distance(position, goals[current_goal_idx]);
        DrawText(TextFormat("Goal %d/%d dist: %.0f", current_goal_idx, (int)goals.size(), dist),
                position.x + 20, position.y + 60, 10, YELLOW);

        // linha da nave até o objetivo atual (visualização dos sensores de direção)
        DrawLineV(position, goals[current_goal_idx], Color{0, 200, 255, 120});
        DrawCircleV(goals[current_goal_idx], 8, Color{0, 200, 255, 120});
    } else if(!food_mode) {
        DrawText(TextFormat("NO GOAL | has_goal=%d size=%d idx=%d", 
                (int)has_goal, (int)goals.size(), current_goal_idx),
                position.x + 20, position.y + 60, 10, RED);
    }

    //draw fov
    float angles[] = { -90, -45, 0, 45, 90 };
    for(int i=0; i<NUM_RAYS; i++) {
        float val = ray_sensors[i];
        Color c = (val > 0) ? Color{255, (unsigned char)(255*(1-val)), 0, 200} : Color{0, 255, 0, 50};
        
        float len = ray_max_dist;
        float ang_rad = facing_angle + angles[i] * DEG2RAD;
        
        //descomente isso para desenhar os cabelinhos sempre
        // Vector2 end = {
        //    position.x + cosf(ang_rad) * len,
        //    position.y + sinf(ang_rad) * len
        //};
        //DrawLineV(position, end, c);
        
        if(val > 0) {
            Vector2 hit = {
                position.x + cosf(ang_rad) * len * (1.0f - val),
                position.y + sinf(ang_rad) * len * (1.0f - val)
            };
            DrawLineV(position, hit, c);
            DrawCircleV(hit, 5, RED);
        }
    }

    //draw current score
    DrawText(TextFormat("Score: %.0f", score), position.x + 20, position.y + 20, 10, GREEN);

    DrawText(TextFormat("Colls: %d", coll_count), position.x + 20, position.y + 40, 10, GREEN);

    if(is_best){
        int best_label_y = (current_mode == GameMode::OwnGoal) ? 80 : 60;
        DrawText("Previous best",position.x + 20, position.y + best_label_y, 15, GREEN);
        DrawCircleLines(position.x, position.y, collisionradius*2, GREEN);
    }
    
}
