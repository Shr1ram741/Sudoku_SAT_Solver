#pragma once
#include <string>
#include <vector>

using Clause = std::vector<int>;
using CNFFormula = std::vector<Clause>;

class CNFParser {
public:
    static CNFFormula parse(const std::string& filename);
};
