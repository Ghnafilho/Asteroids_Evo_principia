#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <iomanip>

using Genome = std::vector<std::vector<double>>;

namespace Persistence {

    // salva geração atual + genomas num arquivo texto
    inline void saveGenomes(const std::string& path, int generation, int bot_count, int asteroid_count, const Genome& genomes) {
        std::ofstream out(path);
        if (!out) return;

        out << std::setprecision(17); // preserva precisao total do double

        out << generation << "\n";
        out << bot_count << "\n";
        out << asteroid_count << "\n";
        out << genomes.size() << "\n";

        for (const auto& genome : genomes) {
            out << genome.size();
            for (double gene : genome) {
                out << " " << gene;
            }
            out << "\n";
        }
    }

    // tenta carregar; retorna true se conseguiu
    inline bool loadGenomes(const std::string& path, int& generation, int& bot_count, int& asteroid_count, Genome& genomes) {
        std::ifstream in(path);
        if (!in) return false;

        if (!(in >> generation >> bot_count >> asteroid_count)) return false;

        size_t genome_count;
        if (!(in >> genome_count)) return false;

        genomes.clear();
        genomes.reserve(genome_count);

        for (size_t i = 0; i < genome_count; i++) {
            size_t gene_count;
            if (!(in >> gene_count)) return false;

            std::vector<double> genome(gene_count);
            for (size_t j = 0; j < gene_count; j++) {
                if (!(in >> genome[j])) return false;
            }
            genomes.push_back(std::move(genome));
        }

        return true;
    }

}