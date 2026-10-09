## Asteroids_Evo
The goal of this project is to create an Asteroids-like game using a simplified self-developed physics engine, and implement an [Evolutionary Algorithm](https://en.wikipedia.org/wiki/Evolutionary_algorithm) to teach an AI to play the game on its own, learning to move forward while avoiding collisions. Developed in C++. Graphics done using [Raylib](https://github.com/raysan5/raylib).

<img width="796" height="494" alt="image" src="https://github.com/user-attachments/assets/7b834b96-6501-46de-839e-0d390e987a4d" />

This project is ongoing and in development.

## How to run
**To compile and run on Windows:** Install either [w64devkit](https://github.com/skeeto/w64devkit), [Windows Terminal](https://github.com/microsoft/terminal) or a similar tool. Navigate to the /src directory, and use ``'make run'``. (If using Windows Terminal, make sure to have OpenGL installed)

**To compile and run on Linux:** Navigate to the /src directory, and use ``'make run'``.
Make sure you have the ``libgl1-mesa-dev`` package installed. If you don't, install it using ``'sudo apt install libgl1-mesa-dev'``.

## How it works

### Robots
Robots piloting spaceships must learn to dodge asteroids and other spaceships in an Asteroids-like simulated space environment. A simple physics engine is implemented to run this simulation. You can also play the game yourself by using the "Toggle Player" button in the UI. Your ship will spawn at the start of the next generation, and can be controlled with the arrow keys. 

Robots are given spaceships which they can control with the same method as the player. During a game tick, the robot must choose whether to send a forward, backward, left or right input. They can also choose to press multiple inputs simultaneously. A generation lasts for a predetermined amount of game ticks, after which the robots will reproduce to evolve. See the evolution section for more details on this.

The robots can sense the world around them through a series of 7 sensors, divided into: 5 sensors that allow them to see the distance to the closest object in 5 directions (raycasts directed foward, left, right, forward-left, and forward-right), one sensor that tells it it's own speed, and one sensor that tells it how aligned its facing direction is with its movement direction (so that it can know if it's moving forward or drifting sideways).

Currently, robots are implemeted as single [Perceptrons](https://en.wikipedia.org/wiki/Perceptron). Each robot has a set of 32 genes; The first 28 serve as weights for the values obtained from the sensors, and the last 4 serve as thresholds for output activation. 

During each game tick, each robot makes the choice of pressing or not an input by taking the values measured from the 7 sensors, multiplying them by the value in the corresponding weight gene, adding them together, and comparing this result against the value in the corresponding threshold gene. If the threshold is crossed, they will press that input, otherwise, they will not.

<img width="630" height="289" alt="image" src="https://github.com/user-attachments/assets/afa3737f-bf43-4f84-9270-97480db76d11" />

*Diagram showing the artificial neuron currently implemented. Sourced from Wikipedia.*

### Evolution
In the first generation, the robots are given randomly generated float values for their genes.

A generation consists of a predetermined amount of game ticks to be simulated. Each simulation begins with a predetermined amount of ships and asteroids, scattered around the screen randomly. During the simulation, robots will earn score by moving forward, and lose score for colliding with asteroids, other ships, or screen edges. To reduce luck as a factor in scoring, each generation simulates 8 worlds in parallel with different randomized starting conditions. Each robot is then given its final score through the average of its performance in all simulations, discarding the best and worst scores (trimmed average).

At the end of a generation, child robots will be generated. For each child, two parents are chosen through a [2's Tournament selection](https://en.wikipedia.org/wiki/Tournament_selection), and they will reproduce sexually. In this case, sexual reproduction means that to generate a child's genome, each child gene is obtained by taking the average of the parent's genes, then applying mutations (a variation of +- 25% in addition to a random flat value) to a random number of genes.

In addition, the best-performing robot will also have a clone of it inserted into the next generation, with no changes to the genome.

After this is done, the new generation will have been built, populated by one clone of the previous best individual and the rest of the invididuals generated using the sexual reproduction method. This new population is then evaluated with the same simulation method, and the selection and reproduction method is applied again, repeating this process ad infinitum.

*todo: 2's tournament diagram here*

The idea is that robots with higher scores will get to reproduce more often, thus propagating their genes, while random mutations allow them to randomly explore new stategies. Over time, the average score of the population should increase, which can be seen in the fitness graph shown in the program. As more generations pass, the robots should get increasingly better at dodging obstacles on their own. 

## TO-DO list
- Improve GUI for choosing parameters/settings during runtime
- Better documentation
- Better logging of evolution data
- More advanced neural networks to evolve
- More advanced/optimized physics
- Dynamic selective pressure
