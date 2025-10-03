#include "sat_solver.h"
#include <algorithm>
#include <iostream>
#include <stack>
#include <cassert>

SATSolver::SATSolver(const CNFFormula& f)
    : formula(f),
      num_vars(0)
{
    // determine number of variables from formula
    for (const auto &cl : formula) {
        for (int lit : cl) {
            int v = std::abs(lit);
            if (v > num_vars) num_vars = v;
        }
    }

    assign.assign(num_vars + 1, 0);
    level.assign(num_vars + 1, -1);
    reason.assign(num_vars + 1, -1);
    activity.assign(num_vars + 1, 0.0);

    init_watches();
}

void SATSolver::init_watches() {
    // initialize watches by watching first two literals of every clause
    for (int i = 0; i < (int)formula.size(); ++i) {
        const Clause &cl = formula[i];
        if (cl.empty()) {
            // empty clause -> immediate UNSAT, but we'll let propagate detect it
            continue;
        }
        watch_literal(cl[0], i);
        if (cl.size() > 1) watch_literal(cl[1], i);
        else watch_literal(cl[0], i); // duplicate watch for unary clause
    }
}

void SATSolver::watch_literal(int lit, int clause_idx) {
    watches[lit].push_back(clause_idx);
}

void SATSolver::unwatch_literal(int lit, int clause_idx) {
    auto it = watches.find(lit);
    if (it == watches.end()) return;
    auto &vec = it->second;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] == clause_idx) {
            vec[i] = vec.back();
            vec.pop_back();
            return;
        }
    }
}

void SATSolver::assign_lit(int lit, int clause_idx) {
    int v = std::abs(lit);
    assign[v] = (lit > 0) ? 1 : -1;
    level[v] = static_cast<int>(trail_lim.size());
    reason[v] = clause_idx;
    trail.push_back(lit);
}

std::optional<int> SATSolver::propagate() {
    // Standard propagation over the trail: process newly assigned literals
    size_t propagate_idx = 0;
    while (propagate_idx < trail.size()) {
        int lit = trail[propagate_idx++];
        int blocking = -lit; // clauses watching -lit may become unit/conflict

        // copy watched list to avoid iterator invalidation while we modify watches
        auto watchers = watches.find(blocking);
        if (watchers == watches.end()) continue;
        std::vector<int> watch_copy = watchers->second;

        for (int ci : watch_copy) {
            const Clause &cl = formula[ci];

            // Check whether clause is satisfied. If yes, nothing to do.
            bool satisfied = false;
            int unassigned_count = 0;
            int last_unassigned = 0;

            for (int l : cl) {
                int v = std::abs(l);
                int8_t a = assign[v];
                if (a == 0) {
                    ++unassigned_count;
                    last_unassigned = l;
                } else {
                    if ((a == 1 && l > 0) || (a == -1 && l < 0)) {
                        satisfied = true;
                        break;
                    }
                }
            }

            if (satisfied) continue;

            if (unassigned_count == 0) {
                // conflict
                return ci;
            } else if (unassigned_count == 1) {
                // unit clause -> assign last_unassigned
                // Avoid re-assigning if already assigned (shouldn't happen)
                int v = std::abs(last_unassigned);
                if (assign[v] == 0) assign_lit(last_unassigned, ci);
            } else {
                // More than one unassigned - in a full implementation we would try to move the watch
                // from 'blocking' to some other literal in the clause. For simplicity we rely on
                // scanning here (works but slower).
                continue;
            }
        }
    }
    return std::nullopt;
}

int SATSolver::pick_branch_var() {
    int best = 0;
    double best_score = -1.0;
    for (int v = 1; v <= num_vars; ++v) {
        if (assign[v] == 0 && activity[v] > best_score) {
            best_score = activity[v];
            best = v;
        }
    }
    if (best != 0) return best;
    // fallback: first unassigned
    for (int v = 1; v <= num_vars; ++v) if (assign[v] == 0) return v;
    return 0;
}

void SATSolver::bump_activity(int var) {
    activity[var] += var_inc;
    // simple rescale if numbers blow up
    if (activity[var] > 1e100) {
        for (int i = 1; i <= num_vars; ++i) activity[i] *= 1e-100;
        var_inc *= 1e-100;
    }
}

std::vector<int> SATSolver::analyze_conflict(int conflict_clause_idx) {
    // Simple first-UIP-like analysis:
    // - Start from conflict clause
    // - Mark variables seen and traverse reasons until we have reduced to 1 literal from current level
    // - Build learned clause as negation of those literals not from current level and one from current level

    std::vector<char> seen(num_vars + 1, 0);
    std::vector<int> learned;
    std::vector<int> stack;

    // push conflict clause literals (we treat them as signed ints)
    for (int l : formula[conflict_clause_idx]) stack.push_back(l);

    int curr_level = static_cast<int>(trail_lim.size());
    int num_levels_in_clause = 0;

    // Mark & count literals from current level
    while (!stack.empty()) {
        int l = stack.back();
        stack.pop_back();
        int v = std::abs(l);
        if (seen[v]) continue;
        seen[v] = 1;
        if (level[v] == curr_level) {
            ++num_levels_in_clause;
        } else {
            // add to learned clause as negation (we will keep these)
            learned.push_back(-l);
        }
        // if v has a reason, add its clause literals to the stack for resolution
        if (reason[v] != -1) {
            for (int rl : formula[reason[v]]) {
                if (std::abs(rl) == v) continue;
                if (!seen[std::abs(rl)]) stack.push_back(rl);
            }
        }
    }

    // If num_levels_in_clause <= 1, we've reached first-UIP and learned is ready.
    // Otherwise, we need to iteratively resolve using last assigned at current level.
    while (num_levels_in_clause > 1) {
        // find last assigned variable on trail that is seen and at current level
        int pivot_var = 0;
        for (auto it = trail.rbegin(); it != trail.rend(); ++it) {
            int lit = *it;
            int v = std::abs(lit);
            if (seen[v] && level[v] == curr_level) { pivot_var = v; break; }
        }
        if (pivot_var == 0) break; // shouldn't happen

        // resolve with reason[pivot_var]
        int rci = reason[pivot_var];
        seen[pivot_var] = 0; // we will remove pivot as it's resolved away
        --num_levels_in_clause;

        if (rci == -1) {
            // pivot was a decision literal (no reason) -> learned clause will contain its negation
            learned.push_back(-( (assign[pivot_var] == 1) ? pivot_var : -pivot_var ));
            continue;
        }

        // add literals from reason clause
        for (int rl : formula[rci]) {
            int v = std::abs(rl);
            if (seen[v]) continue;
            seen[v] = 1;
            if (level[v] == curr_level) ++num_levels_in_clause;
            else learned.push_back(-rl);
        }
    }

    // if learned is empty (degenerate), create a clause that blocks the conflicting assignment
    if (learned.empty()) {
        // fallback: negate all literals of conflict clause
        for (int l : formula[conflict_clause_idx]) learned.push_back(-l);
    }

    // Optionally: bump activity of variables appearing in learned
    for (int l : learned) bump_activity(std::abs(l));

    return learned;
}

void SATSolver::add_learned_clause(const std::vector<int>& cl) {
    formula.push_back(cl);
    int idx = (int)formula.size() - 1;
    // watch the first two literals (duplicate if unary)
    if (!cl.empty()) {
        watch_literal(cl[0], idx);
        if (cl.size() > 1) watch_literal(cl[1], idx);
        else watch_literal(cl[0], idx);
    }
}

void SATSolver::backjump(int target_level) {
    // unassign variables with level > target_level
    while (!trail.empty()) {
        int lit = trail.back();
        int v = std::abs(lit);
        if (level[v] <= target_level) break;
        assign[v] = 0;
        level[v] = -1;
        reason[v] = -1;
        trail.pop_back();
    }
    // pop decision levels
    while ((int)trail_lim.size() > target_level) trail_lim.pop_back();
}

bool SATSolver::solve() {
    // Initial unit propagation for unary clauses
    for (int ci = 0; ci < (int)formula.size(); ++ci) {
        const Clause &cl = formula[ci];
        if (cl.size() == 1) {
            int lit = cl[0];
            int v = std::abs(lit);
            if (assign[v] == 0) assign_lit(lit, ci);
        }
    }

    while (true) {
        auto conflict = propagate();
        if (conflict.has_value()) {
            // conflict at level 0 => UNSAT
            if (trail_lim.empty()) return false;

            // analyze and learn
            std::vector<int> learned = analyze_conflict(*conflict);
            add_learned_clause(learned);

            // compute backjump level = max level among literals in learned except highest (simple heuristic)
            int highest = -1, second_highest = -1;
            for (int l : learned) {
                int v = std::abs(l);
                int lev = (level[v] >= 0) ? level[v] : 0;
                if (lev > highest) { second_highest = highest; highest = lev; }
                else if (lev > second_highest) second_highest = lev;
            }
            int target = std::max(0, second_highest);
            backjump(target);
        } else {
            // decide next variable
            int var = pick_branch_var();
            if (var == 0) return true; // all variables assigned => SAT

            // make decision (branch true by default)
            trail_lim.push_back((int)trail.size());
            assign_lit(var, -1); // -1 marker for decision reason
        }
    }
}

