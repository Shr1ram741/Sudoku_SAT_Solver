def varnum(r, c, d):
    """Map (row, col, digit) → SAT variable (1..729)."""
    return 81 * (r - 1) + 9 * (c - 1) + d

def sudoku_to_cnf(sudoku):
    clauses = []

    # 1. Each cell has at least one number
    for r in range(1, 10):
        for c in range(1, 10):
            clauses.append([varnum(r, c, d) for d in range(1, 10)])

    # 2. Each cell has at most one number
    for r in range(1, 10):
        for c in range(1, 10):
            for d1 in range(1, 10):
                for d2 in range(d1 + 1, 10):
                    clauses.append([-varnum(r, c, d1), -varnum(r, c, d2)])

    # 3. Each number appears once per row
    for r in range(1, 10):
        for d in range(1, 10):
            for c1 in range(1, 10):
                for c2 in range(c1 + 1, 10):
                    clauses.append([-varnum(r, c1, d), -varnum(r, c2, d)])

    # 4. Each number appears once per column
    for c in range(1, 10):
        for d in range(1, 10):
            for r1 in range(1, 10):
                for r2 in range(r1 + 1, 10):
                    clauses.append([-varnum(r1, c, d), -varnum(r2, c, d)])

    # 5. Each number appears once per 3x3 block
    for br in range(0, 3):
        for bc in range(0, 3):
            for d in range(1, 10):
                cells = [(r, c) for r in range(br*3+1, br*3+4)
                                for c in range(bc*3+1, bc*3+4)]
                for i in range(len(cells)):
                    for j in range(i+1, len(cells)):
                        r1, c1 = cells[i]
                        r2, c2 = cells[j]
                        clauses.append([-varnum(r1, c1, d), -varnum(r2, c2, d)])

    # 6. Encode given numbers
    for r in range(1, 10):
        for c in range(1, 10):
            d = sudoku[r-1][c-1]
            if d != 0:
                clauses.append([varnum(r, c, d)])

    # Convert to DIMACS CNF format
    n_vars = 9*9*9
    dimacs = []
    dimacs.append(f"p cnf {n_vars} {len(clauses)}")
    for clause in clauses:
        dimacs.append(" ".join(map(str, clause)) + " 0")

    return "\n".join(dimacs)

if __name__ == "__main__":
    # Example Sudoku (0 = blank)
    sudoku = [
        [5,3,0, 0,7,0, 0,0,0],
        [6,0,0, 1,9,5, 0,0,0],
        [0,9,8, 0,0,0, 0,6,0],

        [8,0,0, 0,6,0, 0,0,3],
        [4,0,0, 8,0,3, 0,0,1],
        [7,0,0, 0,2,0, 0,0,6],

        [0,6,0, 0,0,0, 2,8,0],
        [0,0,0, 4,1,9, 0,0,5],
        [0,0,0, 0,8,0, 0,7,9]
    ]

    cnf = sudoku_to_cnf(sudoku)
    print(cnf)

