#ifndef MODE_H
#define MODE_H

// versao do codigo escolhida no menu
enum class GameMode {
    SharedGoals,    // todos os robos seguem os mesmos 4 objetivos fixos nos cantos
    OwnGoal         // cada robo sorteia o proprio objetivo num raio ao seu redor
};

// modo ativo: so e alterado no menu, quando nenhuma simulacao esta rodando
inline GameMode current_mode = GameMode::SharedGoals;

#endif
