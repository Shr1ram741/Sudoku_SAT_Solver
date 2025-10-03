#pragma once
#include "cnf_parser.h"
#include <vector>
#include <unordered_map>
#include <optional>

class SATSolver {
public:
    explicit SATSolver(const CNFFormula& f);

    // Run the solver. Returns true if satisfiable.
    bool solve();

    // If solved SAT, this returns an assignment vector indexed by variable (1..n).
    // 0 = unassigned, 1 = true, -1 = false
    const std::vector<int8_t>& get_assignment() const { return assign; }

private:
    CNFFormula formula;
    int num_vars;

    // current assignment state
    std::vector<int8_t> assign;    // size num_vars+1, 0/1/-1
    std::vector<int> level;        // decision level for each var
    std::vector<int> reason;       // clause index that implied the assignment, -1 if decision

    // trail & decision stack
    std::vector<int> trail;        // assigned literals in order (signed ints)
    std::vector<int> trail_lim;    // indexes in trail where each decision started

    // watched lists: literal -> list of clause indices
    std::unordered_map<int, std::vector<int>> watches;

    // variable activity (simple heuristic)
    std::vector<double> activity;
    double var_inc = 1.0;

    // internal helpers
    void init_watches();
    void watch_literal(int lit, int clause_idx);
    void unwatch_literal(int lit, int clause_idx);

    void assign_lit(int lit, int clause_idx);
    std::optional<int> propagate(); // returns conflicting clause index or nullopt

    int pick_branch_var(); // returns variable number (positive) or 0 if all assigned

    std::vector<int> analyze_conflict(int conflict_clause_idx); // returns learned clause (list of lits)
    void add_learned_clause(const std::vector<int>& cl);
    void backjump(int target_level);

    void bump_activity(int var);
};