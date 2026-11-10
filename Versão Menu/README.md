# Versão Menu

Junta as duas versões do projeto num único programa. Ao abrir, um menu permite escolher qual delas rodar:

| Versão | Objetivos | Sensores de objetivo | Genocídio | Save / log |
|---|---|---|---|---|
| **Objetivos fixos nos cantos** (pasta `Asteroids_Evo-main (1)`) | os mesmos 4 objetivos fixos nos cantos, em sequência; +10000 por objetivo | direção e alinhamento até o objetivo | `genocidio()` a cada 60 gerações | `population_cantos.txt` / `log_cantos.txt` |
| **Cada robô com meta própria** (pasta `Versão que cada robô tem uma meta própria`) | cada robô sorteia um objetivo a 300 px; +5000 x alinhamento por objetivo | distância e direção até o objetivo | `genocidio1()` a cada 100 gerações | `population_meta_propria.txt` / `log_meta_propria.txt` |

Na versão de meta própria, as saídas de girar só ativam acima de 0.3 (como na versão original).

No menu também dá para escolher entre **continuar do save** ou **começar do zero**. Durante a simulação, o botão **Menu** volta para a tela inicial; o progresso até o início da geração atual já fica salvo.

O botão **Ambiente interativo** de cada card (aparece quando existe save) coloca uma única nave, o melhor robô daquele save, num ambiente livre: cada clique na tela adiciona uma comida, a nave as busca na ordem dos cliques e não morre. O slider **Asteroid Count** muda os asteroides na hora, e os botões Graphics, Debug, Player, FPS cap e Pause funcionam como no treino.

O botão **Features** esconde ou mostra os controles nas telas de simulação; o botão **Menu** fica sempre visível.

## Como rodar

Igual às outras versões: entre na pasta `src` e rode `make run` (Linux/WSL ou w64devkit).

Teste do ambiente interativo (dentro de `tests/`, no Linux/WSL):

```
g++ -std=c++20 -I../raylib/include -I../src interactive_test.cpp ../src/ship.cpp ../src/entity.cpp ../src/world.cpp ../src/asteroid.cpp ../src/bot.cpp ../src/rng.cpp ../src/spatial_hash.cpp -L../raylib/lib -lraylib_linux -lGL -lm -lpthread -ldl -lrt -lX11 -o /tmp/interactive_test && /tmp/interactive_test
```

## Onde fica cada diferença no código

O modo escolhido fica em `current_mode` (`src/mode.hpp`). Os trechos que dependem dele:

- `ship.cpp`: objetivo (`set_goals` / `generate_goal`), pontuação, sensores de objetivo e desenho de debug
- `bot.cpp`: limiar de rotação
- `world.cpp`: criação e distribuição dos objetivos
- `main.cpp`: menu, arquivos de save/log e genocídio (tabela `MODES`)
