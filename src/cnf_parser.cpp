#include "cnf_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>

CNFFormula CNFParser::parse(const std::string& filename) {
    CNFFormula formula;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == 'c' || line[0] == 'p') continue;
        std::istringstream iss(line);
        int lit;
        Clause clause;
        while (iss >> lit && lit != 0) {
            clause.push_back(lit);
        }
        if (!clause.empty()) formula.push_back(clause);
    }
    return formula;
}
