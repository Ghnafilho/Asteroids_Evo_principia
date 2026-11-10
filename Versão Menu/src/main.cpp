#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <ctime>
#include <memory>
#include <algorithm>
#include <thread>
#include <format>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "asteroid.hpp"
#include "ship.hpp"
#include "fitnessgraph.cpp"
#include "entity.hpp"
#include "world.hpp"
#include "evolution.hpp"
#include "bot.hpp"
#include "threads.cpp"
#include "persistence.hpp"
#include "mode.hpp"

// --- SETTINGS ---


#define CELL_SIZE 32
#define worldWidth 1600
#define worldHeight 1000
#define GENOME_SIZE 130  // (9 sensores + 1 bias) x 4 saidas
#define WORLD_COUNT 8

#define DEFAULT_GEN_DURATION 1000
#define DEFAULT_BOT_COUNT 10
#define DEFAULT_ASTEROID_COUNT 10

using namespace std;
using Genome = vector<vector<double>>;
using ScoredGenome = pair<vector<double>, float>;
using ScoredGenomeList = vector<ScoredGenome>;

// --- VERSIONS ---

// everything that changes between the versions shown in the menu
struct ModeInfo {
    GameMode mode;
    const char* name;
    const char* description;
    string save_file;
    string log_file;
    int genocide_interval;      // run a genocide every N generations
    bool random_genocide;       // true: genocidio() (random individuals), false: genocidio1() (random parents + crossover)
};

const ModeInfo MODES[] = {
    {
        GameMode::SharedGoals,
        "Objetivos fixos nos cantos",
        "Todos os robôs perseguem os mesmos 4\n"
        "objetivos, nos cantos da tela, em sequência.\n\n"
        "Sensores: direção e alinhamento até o\n"
        "objetivo atual.\n"
        "Recompensa: +10000 por objetivo alcançado.\n"
        "Genocídio a cada 60 gerações: mantém o\n"
        "melhor e sorteia o resto da população.",
        "population_cantos.txt",
        "log_cantos.txt",
        60,
        true
    },
    {
        GameMode::OwnGoal,
        "Cada robô com meta própria",
        "Cada robô sorteia o próprio objetivo a 300 px\n"
        "de distância; ao alcançar, ganha um novo.\n\n"
        "Sensores: distância e direção até o objetivo.\n"
        "Recompensa: +5000 x alinhamento por objetivo.\n"
        "Girar exige ativação acima de 0.3.\n"
        "Genocídio a cada 100 gerações: mantém o\n"
        "melhor e cruza pais aleatórios.",
        "population_meta_propria.txt",
        "log_meta_propria.txt",
        100,
        false
    }
};
const int MODE_COUNT = sizeof(MODES) / sizeof(MODES[0]);

// what the menu shows about each version's save file
struct SaveInfo {
    bool exists = false;
    int generation = 0;
    int bot_count = 0;
};

SaveInfo read_save_info(const string& path){
    SaveInfo info;
    int asteroids;
    Genome genomes;
    info.exists = Persistence::loadGenomes(path, info.generation, info.bot_count, asteroids, genomes);
    return info;
}


int main() {
    // init ------------------------------------------------------------------------------------
    SetRandomSeed((time(NULL)));
    InitWindow(worldWidth, worldHeight, "RonaldoBots");
    SetTargetFPS(60);

    // simulation state (reset every time a version is started from the menu)
    float gen_duration = DEFAULT_GEN_DURATION;
    int bot_count = DEFAULT_BOT_COUNT;
    int asteroid_count = DEFAULT_ASTEROID_COUNT;
    int generation = 1;

    bool draw_graphics = true;
    bool player_active = false;
    bool render_anything = true;
    bool draw_debug = true;
    bool capped_fps = true;
    bool paused = false;
    int next_bot_count = bot_count;
    int next_asteroid_count = asteroid_count;
    Fitness_graph avg_graph = Fitness_graph(10,130,300,100);
    Fitness_graph best_graph = Fitness_graph(10,240,300,100,"BEST GEN FITNESS");
    ofstream logfile;

    // menu state
    bool in_menu = true;
    bool back_to_menu = false;
    int start_option = 0;           // 0 = continue from save, 1 = start from scratch
    const ModeInfo* mode_info = &MODES[0];
    SaveInfo saves[MODE_COUNT];
    for(int i = 0; i < MODE_COUNT; i++) saves[i] = read_save_info(MODES[i].save_file);

    //------------------------------------------------------------------------------------------


    // load textures ---------------------------------------------------------------------------

    vector<Texture2D> textures;
    textures.emplace_back(LoadTexture("../assets/millenium.png"));
    textures.emplace_back(LoadTexture("../assets/asteroid.png"));
    textures.emplace_back(LoadTexture("../assets/xwing.png"));

    //------------------------------------------------------------------------------------------

    // create universe -------------------------------------------------------------------------

    vector<unique_ptr<World>> universe;

    // Initialize Thread Pool
    vector<Worker> workers(WORLD_COUNT);
    for (int i = 0; i < WORLD_COUNT; i++) {
        workers[i].start();
    }

    // starts the chosen version, loading its save unless start_fresh is set
    auto start_simulation = [&](const ModeInfo& info, bool start_fresh){
        mode_info = &info;
        current_mode = info.mode;

        gen_duration = DEFAULT_GEN_DURATION;
        bot_count = DEFAULT_BOT_COUNT;
        asteroid_count = DEFAULT_ASTEROID_COUNT;
        generation = 1;

        int saved_generation, saved_bot_count, saved_asteroid_count;
        Genome loaded_genomes;
        bool has_save = !start_fresh && Persistence::loadGenomes(info.save_file, saved_generation, saved_bot_count, saved_asteroid_count, loaded_genomes);
        if(has_save){
            generation = saved_generation;
            bot_count = saved_bot_count;
            asteroid_count = saved_asteroid_count;
        }
        next_bot_count = bot_count;
        next_asteroid_count = asteroid_count;
        paused = false;

        avg_graph = Fitness_graph(10,130,300,100);
        best_graph = Fitness_graph(10,240,300,100,"BEST GEN FITNESS");

        logfile.open(info.log_file, has_save ? ios::app : ios::trunc);
        logfile << (has_save ? "Retomando de save existente.\n" : "Nenhum save encontrado. Iniciando do zero.\n");
        logfile << "-- STARTING NEW SIMULATION --\n";
        logfile << "Initial params:\n";
        logfile << "VERSION: " << info.name << "\n";
        logfile << "WORLD_COUNT: " << WORLD_COUNT << "\n";
        logfile << "GENOME_SIZE: " << GENOME_SIZE << "\n";
        logfile << "WORLD WIDTH: " << worldWidth << "\n";
        logfile << "WORLD HEIGHT: " << worldHeight << "\n";
        logfile << "GENERATION DURATION: " << gen_duration << "\n";

        // create first worlds with random genomes
        for(int i = 0; i < WORLD_COUNT; i++) {
            bool spawn = false;
            if(i == 0) spawn = player_active;
            if (has_save) {
                universe.push_back(make_unique<World>(bot_count, asteroid_count, worldWidth, worldHeight, CELL_SIZE, textures, gen_duration, loaded_genomes, spawn));
            } else {
                universe.push_back(make_unique<World>(bot_count, asteroid_count, worldWidth, worldHeight, CELL_SIZE, textures, gen_duration, player_active));
            }
        }

        logfile << format("starting generation {:d} ------------------------------- \n", generation);
        in_menu = false;
    };

    //------------------------------------------------------------------------------------------

    // main loop -------------------------------------------------------------------------------
    while (!WindowShouldClose()) {

        // Menu --------------------------------------------------------------------------------
        if(in_menu){
            int chosen = -1;

            BeginDrawing();
            ClearBackground(BLACK);

            const char* title = "RonaldoBots";
            DrawText(title, (worldWidth - MeasureText(title, 60))/2, 90, 60, GREEN);
            const char* subtitle = "Escolha qual versão do código rodar";
            DrawText(subtitle, (worldWidth - MeasureText(subtitle, 25))/2, 170, 25, LIME);

            // one card per version
            const int card_w = 560, card_h = 470, card_gap = 80, card_y = 250;
            const int first_card_x = (worldWidth - (MODE_COUNT*card_w + (MODE_COUNT-1)*card_gap))/2;
            for(int i = 0; i < MODE_COUNT; i++){
                int x = first_card_x + i*(card_w + card_gap);
                DrawRectangle(x, card_y, card_w, card_h, Fade(DARKGREEN, 0.2f));
                DrawRectangleLines(x, card_y, card_w, card_h, GREEN);

                DrawText(MODES[i].name, x + 25, card_y + 25, 30, GREEN);
                DrawText(MODES[i].description, x + 25, card_y + 80, 20, RAYWHITE);

                if(saves[i].exists){
                    DrawText(TextFormat("Save: geração %d, %d robôs", saves[i].generation, saves[i].bot_count), x + 25, card_y + 330, 20, YELLOW);
                }else{
                    DrawText("Sem save: começa do zero", x + 25, card_y + 330, 20, GRAY);
                }
                DrawText(TextFormat("Arquivo: %s", MODES[i].save_file.c_str()), x + 25, card_y + 358, 15, GRAY);

                GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
                if(GuiButton({(float)x + 25, (float)card_y + card_h - 75, (float)card_w - 50, 50}, "Rodar esta versão")) chosen = i;
                GuiSetStyle(DEFAULT, TEXT_SIZE, 10);
            }

            // start option
            const int option_y = card_y + card_h + 50;
            const char* option_label = "Ao iniciar:";
            DrawText(option_label, (worldWidth - MeasureText(option_label, 20))/2, option_y, 20, LIME);
            GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
            GuiToggleGroup({(float)worldWidth/2 - 300, (float)option_y + 35, 300, 45}, "Continuar do save;Começar do zero", &start_option);
            GuiSetStyle(DEFAULT, TEXT_SIZE, 10);
            if(start_option == 1){
                const char* warning = "Atenção: ao fim da primeira geração, o save antigo dessa versão é substituído.";
                DrawText(warning, (worldWidth - MeasureText(warning, 18))/2, option_y + 100, 18, ORANGE);
            }

            EndDrawing();

            if(chosen >= 0) start_simulation(MODES[chosen], start_option == 1);
            continue;
        }

        //--------------------------------------------------------------------------------------

        //update worlds if not paused
        if(!paused){
            // Wake all threads and assign them their specific world for this tick
            for(int i = 0; i < WORLD_COUNT ; i++) {
                workers[i].triggerUpdate(universe[i].get());
            }

            // Wait for all physics simulations to finish before proceeding
            for(int i = 0; i < WORLD_COUNT ; i++) {
                workers[i].waitDone();
            }
        }

        //on epoch finish:
        if (all_of(universe.begin(), universe.end(), [&](auto& a) { return a->isFinished(); })){

            //get results from all worlds
            vector<ScoredGenomeList> all_results;
            for(int i = 0; i < WORLD_COUNT; i++) {
                all_results.push_back(universe[i]->getResult());
            }

            //save genomes from first world
            ScoredGenomeList avgScores = all_results[0];

            //get trimmed avg for each bot
            for (int j = 0; j < bot_count; j++) {
                vector<float> bot_scores;

                //get this bot's scores from all worlds
                for (int i = 0; i < WORLD_COUNT; i++) {
                    bot_scores.push_back(all_results[i][j].second);
                }

                //sort the scores
                sort(bot_scores.begin(), bot_scores.end());

                float score_sum = 0.0f;
                int valid_scores = 0;

                //ignore the highest and lowest
                for (int k = 1; k < WORLD_COUNT - 1; k++) {
                    score_sum += bot_scores[k];
                    valid_scores++;
                }

                //put the avg in results list
                avgScores[j].second = score_sum/valid_scores;

                logfile << format("avg score of bot {:d} in generation {:d} {:f}\n", j, generation, avgScores[j].second);
            }

            //free the universe
            universe.clear();

            //add info to avg graph
            float accum = 0;
            for (int i=0; i<bot_count; i++){
                accum += avgScores[i].second;
            }
            accum /= bot_count*1.0;
            logfile << format("generation {:d} avg score: {:f}\n", generation, accum);
            avg_graph.add_datapoint(accum);

            //add info to best graph
            float best_score = 0;
            for (int i=0; i<bot_count; i++){
                if(avgScores[i].second > best_score) best_score = avgScores[i].second;
            }
            logfile << format("generation {:d} best score: {:f}\n", generation, best_score);
            best_graph.add_datapoint(best_score);
            //use avg to evolve algorithm
            bot_count = next_bot_count;
            asteroid_count = next_asteroid_count;
            Evo evo(bot_count, avgScores, GENOME_SIZE);
            Genome next_genomes;
            if(generation % mode_info->genocide_interval == 0){
                next_genomes = mode_info->random_genocide ? evo.genocidio() : evo.genocidio1();
            }
            else{
                next_genomes = evo.repopulation();
            }

                Persistence::saveGenomes(mode_info->save_file, generation + 1, bot_count, next_asteroid_count, next_genomes);

            //make new universes
            for(int i = 0; i < WORLD_COUNT; i++) {
                bool spawn = false;
                if(i == 0) spawn = player_active;
                universe.push_back(std::make_unique<World>(bot_count, asteroid_count, worldWidth, worldHeight, CELL_SIZE, textures, gen_duration, next_genomes, spawn));
            }
            generation++;
            logfile << format("starting generation {:d} ------------------------------- \n", generation);
        }

        // Draw --------------------------------------------------------------------------------

        BeginDrawing();
        if(render_anything){
            ClearBackground(BLACK);

            if(draw_graphics){
                universe[0]->Draw();
                if(draw_debug) universe[0]->DrawExtra();
            }

            // GUI ----------------------------------------------------------------------------
            DrawFPS(worldWidth - 100, 60);
            avg_graph.Draw();
            best_graph.Draw();
            DrawText(TextFormat("GENERATION: %i", generation), 10, 50, 20, GREEN);
            DrawText(TextFormat("TIME: %i", universe[0]->getTime()), 10, 85, 20, GREEN);
            DrawText(TextFormat("WORLDS: %d", WORLD_COUNT), 200, 50, 20, GREEN);
            DrawText(TextFormat("ALIVE: %d", universe[0]->getAlive()), 320, 50, 10, GREEN);
            DrawText(TextFormat("VERSÃO: %s", mode_info->name), 10, 110, 15, YELLOW);

            GuiSliderBar({120, 20, 200, 20 }, "Generation Duration", TextFormat("%.0f", gen_duration), &gen_duration, 250, 3000);
            float slider_bot_count = next_bot_count;

            GuiSliderBar({410, 20, 200, 20 }, "Bot Count", TextFormat("%.0f", slider_bot_count), &slider_bot_count, 2, 40);
            next_bot_count = int(slider_bot_count);
            float slider_asteroid_count = next_asteroid_count;

            GuiSliderBar({710, 20, 200, 20 }, "Asteroid Count", TextFormat("%.0f", slider_asteroid_count), &slider_asteroid_count, 1, 20);
            next_asteroid_count = int(slider_asteroid_count);

            if (GuiButton({940, 20, 120, 30 }, "Toggle Graphics")) draw_graphics = !draw_graphics;
            if (GuiButton({1080, 20, 100, 30 }, "Toggle Debug")) draw_debug = !draw_debug;
            if (GuiButton({1190, 20, 100, 30 }, "Toggle Player")) player_active = !player_active;
            if (GuiButton({1300, 20, 120, 30 }, "Toggle FPS cap")){
                if(capped_fps){
                    SetTargetFPS(0);
                }else{
                    SetTargetFPS(60);
                }
                capped_fps = !capped_fps;
            }
            if (GuiButton({1450, 20, 50, 30 }, "Pause")){
                paused = !paused;
            }
            if (GuiButton({1510, 20, 70, 30 }, "Menu")){
                back_to_menu = true;
            }
            //----------------------------------------------------------------------------------
        }
        EndDrawing();

        // go back to the menu: progress up to the start of this generation is already saved
        if(back_to_menu){
            back_to_menu = false;
            universe.clear();
            logfile << format("returned to menu during generation {:d}\n", generation);
            logfile.close();
            if(!capped_fps){
                SetTargetFPS(60);
                capped_fps = true;
            }
            for(int i = 0; i < MODE_COUNT; i++) saves[i] = read_save_info(MODES[i].save_file);
            start_option = 0;   // never keep "start from scratch" selected, so a save isn't overwritten by accident
            in_menu = true;
        }

        //--------------------------------------------------------------------------------------
    }
    //------------------------------------------------------------------------------------------

    for (auto& w : workers) {
        w.stop();
    }

    CloseWindow();

    return 0;
}
