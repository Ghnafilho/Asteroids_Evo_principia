
#include "bot.hpp"
#include <cmath>
#include <vector>
#include "ship.hpp"
#include "rng.hpp"

using namespace std;

//arquiteture -------------------------------------------------------------------------------
static const int INPUT_SIZE = 9;
static const int HIDDEN1_SIZE = 6;
static const int HIDDEN2_SIZE = 6;
static const int OUTPUT_SIZE = 4;


// constructors -----------------------------------------------------------------------------

Bot::Bot(Ship* body, vector<double> dna)
{
    ship = body;
    score = 0;
    body->facing_angle = GetRandomValue(0,359) * DEG2RAD;
    genome = dna;
}

// getters & setters ------------------------------------------------------------------------

vector<double> Bot::get_genome(){
    return(genome);
}

void Bot::set_genome(vector<double> new_genome){
    genome.clear();
    genome = new_genome;
}

bool Bot::get_alive_status(){
    return ship->active;
}

float Bot::get_coll_radius(){
    return(ship->get_collRadius());
}

// methods ----------------------------------------------------------------------------------

//after-round score post-processing
float Bot::get_score(){
    score = ship->score;
    return score;
}    

/*MLP --------------------------------------------------------------------------------
    Structure:
    54 -> First layer
    6 - > Bias first layer
    36 - > Second layer
    6 - > Bias second layer
    24 -> output
    4 -> Bias output
    */



//decide which way to move
vector<bool> Bot::movement_decision()
{
    vector<double> sensors = ship->getSensors();

    vector<bool> output;

    //Verify if there is the 9 inputs
    if(sensors.size() != INPUT_SIZE){
        return vector<bool>(OUTPUT_SIZE, false);
    }

    // Verifiy if the Genome size is 130
    const int EXPECTED_GENOME_SIZE =
        INPUT_SIZE * HIDDEN1_SIZE +
        HIDDEN1_SIZE +
        HIDDEN1_SIZE * HIDDEN2_SIZE +
        HIDDEN2_SIZE +
        HIDDEN2_SIZE * OUTPUT_SIZE +
        OUTPUT_SIZE;

    if(genome.size() != EXPECTED_GENOME_SIZE){
        return vector<bool>(OUTPUT_SIZE, false);
    }

    //First layer -----------------------------------------------------------------------------

    vector<double> hidden1(HIDDEN1_SIZE);
    int pos = 0;

    for(int i = 0; i < HIDDEN1_SIZE; i++){
        double weighted_sum = 0.0;

        for(int j = 0; j < INPUT_SIZE; j++){
            weighted_sum += sensors[j] * genome[pos];
            pos++;
        }

        // Bias
        weighted_sum += genome[54 + i];

        // Activation function
        hidden1[i] = tanh(weighted_sum);
    }

    //Second layer -----------------------------------------------------------------------------
    
    vector<double> hidden2(HIDDEN2_SIZE);
    pos = 60;

    for(int i = 0; i < HIDDEN2_SIZE; i++){
        double weighted_sum = 0.0;

        for(int j = 0; j < HIDDEN1_SIZE; j++){
            weighted_sum += hidden1[j] * genome[pos];
            pos++;
        }

        // Bias
        weighted_sum += genome[96 + i];

        //Activation function
        hidden2[i] = tanh(weighted_sum);
    }

    //Output layer -------------------------------------------------------------------------------

    pos = 102;

    for(int i = 0; i < OUTPUT_SIZE; i++){
        double weighted_sum = 0.0;

        for(int j = 0; j < HIDDEN2_SIZE; j++){
            weighted_sum += hidden2[j] * genome[pos];
            pos++;
        }

        // Bias
        weighted_sum += genome[126 + i];

        // Activation function
        double activated_output = tanh(weighted_sum);

        if(activated_output > 0.0){
            output.push_back(true);
        }
        else{
            output.push_back(false);
        }
    }

    return output;
}

void Bot::movement(){
    vector<bool> inputs = movement_decision();
    ship->movement(inputs);
}