#include "ship.hpp"
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
    
    Goal_Distance,
    Goal_Direction,

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
    goal_reach_radius = 50.0f;
    goal_reward = 5000.0f;
    killable               = false;
    is_best                = false;
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
    
    if(coll_count > 4 && killable == false){
        killable = true;
    }

    if(type == 0) movement(scan_inputs());

    angularvelocity *= angular_drag;
    speeds = {
        speeds.x * (float)drag,
        speeds.y * (float)drag
    };

    if(angularvelocity > max_angular_velocity)
        angularvelocity = max_angular_velocity;

    if(angularvelocity < -max_angular_velocity)
        angularvelocity = -max_angular_velocity;

    desired_movement_accumulator += abs_speed * alignment_coefficient;

    if(has_goal){
        float current_dist = Vector2Distance(position, current_goal);

        if(current_dist <= goal_reach_radius){
            desired_movement_accumulator2 += goal_reward;
            generate_goal();
        }
    }

    score = desired_movement_accumulator * (1 - 0.15 * coll_count)
            + desired_movement_accumulator2;

    if(score < 0)
        score = 0;
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
    if(has_goal){
        Vector2 to_goal = {
            current_goal.x - position.x,
            current_goal.y - position.y
        };

        float goal_distance = Vector2Length(to_goal);

        float normalized_distance =
            goal_distance / goal_generation_radius;

        if(normalized_distance > 1.0f)
            normalized_distance = 1.0f;

        float goal_angle = atan2f(to_goal.y, to_goal.x);

        float relative_angle = goal_angle - facing_angle;

        while(relative_angle > PI)
            relative_angle -= 2.0f * PI;

        while(relative_angle < -PI)
            relative_angle += 2.0f * PI;

        s[(int)SensorType::Goal_Distance] = normalized_distance;
        s[(int)SensorType::Goal_Direction] = relative_angle / PI;
    }
    else{
        s[(int)SensorType::Goal_Distance] = 0.0;
        s[(int)SensorType::Goal_Direction] = 0.0;
    }

    return s;
}


void Ship::DrawExtra(){
    if(!active) return;

    Entity::DrawExtra();

    if(has_goal){
        float dist = Vector2Distance(position, current_goal);

        DrawCircleV(
            current_goal,
            goal_reach_radius,
            {255, 255, 0, 60}
        );

        DrawCircleLines(
            current_goal.x,
            current_goal.y,
            goal_reach_radius,
            YELLOW
        );

        DrawCircleV(
            current_goal,
            8,
            YELLOW
        );

        DrawLineV(
            position,
            current_goal,
            YELLOW
        );

        DrawText(
            TextFormat("Goal dist: %.0f", dist),
            position.x + 20,
            position.y + 60,
            10,
            YELLOW
        );
    }

    float angles[] = { -90, -45, 0, 45, 90 };

    for(int i = 0; i < NUM_RAYS; i++) {
        float val = ray_sensors[i];

        Color c = (val > 0)
            ? Color{255, (unsigned char)(255 * (1-val)), 0, 200}
            : Color{0, 255, 0, 50};

        float len = ray_max_dist;
        float ang_rad = facing_angle + angles[i] * DEG2RAD;

        if(val > 0) {
            Vector2 hit = {
                position.x + cosf(ang_rad) * len * (1.0f - val),
                position.y + sinf(ang_rad) * len * (1.0f - val)
            };

            DrawLineV(position, hit, c);
            DrawCircleV(hit, 5, RED);
        }
    }

    DrawText(
        TextFormat("Score: %.0f", score),
        position.x + 20,
        position.y + 20,
        10,
        GREEN
    );

    DrawText(
        TextFormat("Colls: %d", coll_count),
        position.x + 20,
        position.y + 40,
        10,
        GREEN
    );

    if(is_best){
        DrawText(
            "Previous best",
            position.x + 20,
            position.y + 80,
            15,
            GREEN
        );

        DrawCircleLines(
            position.x,
            position.y,
            collisionradius * 2,
            GREEN
        );
    }
}