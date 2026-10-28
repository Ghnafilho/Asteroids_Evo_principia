#ifndef SHIP_H
#define SHIP_H

#include <raylib.h>
#include <vector>
#include "entity.hpp"

class Ship : public Entity {
private:
    // Sensor Configurations
    static const int NUM_RAYS = 5;
    float ray_max_dist;             // Maximum vision range
    std::vector<float> ray_sensors; // Stores current values (0.0 to 1.0)
    std::vector<Vector2> goals;
    int current_goal_idx;
    bool has_goal;

public:
    float acceleration;             // linear acceleration
    float drag;                     // linear drag
    float angular_acceleration;     // angular acceleration
    float angular_drag;             // angular drag
    float desired_movement_accumulator;
    float desired_movement_accumulator2;
    float score;                   
    float max_angular_velocity;     // angular speed cap
    bool is_best;
    
    Vector2 current_goal;
    float goal_generation_radius;
    float goal_reach_radius;
    float goal_reward;

    // Constructor
    Ship(float x, float y, int window_w, int window_h, Texture2D& ship_tex, unsigned int id, std::function<void(void)> callback);

    // Overridden methods
    void update() override;
    void DrawExtra() override;
    
    //Extra goal methods
    void set_goals(std::vector<Vector2> new_goals);
    std::vector<Vector2> get_goals();
    int get_current_goal_idx();

    bool get_has_goal();

    // Ship specific methods
    virtual std::vector<bool> scan_inputs();
    void movement(std::vector<bool> inputs);
    void set_best_ship();
    void reset_sensors();
    void sense_walls();
    void update_sensor_with_entity(Entity* other);
    
    // Returns normalized sensor data for Neural Network input
    std::vector<double> getSensors() const;
    void generate_goal();
    Vector2 get_goal() const;
    float get_goal_distance() const;
};

#endif