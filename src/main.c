#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_SYMBOL_LEN 50
#define EPSILON "eps"   // Use "eps" instead of "ε" if you have encoding issues.
#define END_MARKER "$"

typedef struct Symbol {
    char name[MAX_SYMBOL_LEN];
    bool is_terminal;
} Symbol;

typedef struct Production {
    Symbol **symbols;
    int length;
} Production;

typedef struct NonTerminal {
    char name[MAX_SYMBOL_LEN];
    Production **productions;
    int production_count;
    struct NonTerminal *next;
} NonTerminal;

typedef struct CFG {
    NonTerminal *non_terminals;
    Symbol **terminals;
    int terminal_count;
    NonTerminal *start_symbol;
} CFG;

typedef struct FirstSet {
    // Instead of creating new symbols, we use a pointer to the canonical non-terminal name.
    char name[MAX_SYMBOL_LEN];
    Symbol **symbols;
    int count;
} FirstSet;

typedef struct FollowSet {
    char name[MAX_SYMBOL_LEN];
    Symbol **symbols;
    int count;
} FollowSet;

CFG *cfg = NULL;
FirstSet *first_sets = NULL;
FollowSet *follow_sets = NULL;

// -------------------------
// Utility Functions
// -------------------------

// Check if a symbol is already in a set (by name).
bool symbol_in_set(Symbol *s, Symbol **set, int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(set[i]->name, s->name) == 0)
            return true;
    }
    return false;
}

// Add a symbol to a set if not already present.
bool add_symbol_to_set(Symbol *s, Symbol ***set, int *count) {
    if (!symbol_in_set(s, *set, *count)) {
        *set = realloc(*set, sizeof(Symbol*) * ((*count) + 1));
        (*set)[(*count)++] = s;
        return true;
    }
    return false;
}

// Trim leading and trailing whitespace.
char *trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0)
        return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return str;
}

// -------------------------
// Symbol and Production Creation
// -------------------------

Symbol *create_symbol(const char *name, bool is_terminal) {
    Symbol *s = malloc(sizeof(Symbol));
    strncpy(s->name, name, MAX_SYMBOL_LEN);
    s->is_terminal = is_terminal;
    return s;
}

Production *create_production(Symbol **symbols, int length) {
    Production *p = malloc(sizeof(Production));
    p->symbols = symbols;
    p->length = length;
    return p;
}

// -------------------------
// CFG Data Structures Functions
// -------------------------

// Find a non-terminal in the CFG by name.
NonTerminal *find_non_terminal(const char *name) {
    NonTerminal *current = cfg->non_terminals;
    while (current) {
        if (strcmp(current->name, name) == 0)
            return current;
        current = current->next;
    }
    return NULL;
}

// Find a terminal in the CFG by name.
Symbol *find_terminal(const char *name) {
    for (int i = 0; i < cfg->terminal_count; i++) {
        if (strcmp(cfg->terminals[i]->name, name) == 0)
            return cfg->terminals[i];
    }
    return NULL;
}

void add_non_terminal(const char *name) {
    if (find_non_terminal(name))
        return;
    NonTerminal *nt = malloc(sizeof(NonTerminal));
    strncpy(nt->name, name, MAX_SYMBOL_LEN);
    nt->productions = NULL;
    nt->production_count = 0;
    nt->next = cfg->non_terminals;
    cfg->non_terminals = nt;
}

void add_production(NonTerminal *nt, Production *p) {
    nt->productions = realloc(nt->productions, sizeof(Production *) * (nt->production_count + 1));
    nt->productions[nt->production_count++] = p;
}

// -------------------------
// CFG Parsing
// -------------------------

CFG *parse_cfg(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    cfg = malloc(sizeof(CFG));
    cfg->non_terminals = NULL;
    cfg->terminals = NULL;
    cfg->terminal_count = 0;
    cfg->start_symbol = NULL;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strlen(line) < 3 || line[0] == '#') continue;
        char *arrow = strstr(line, "->");
        if (!arrow) continue;

        *arrow = '\0';
        char *lhs = trim(line);
        char *rhs = trim(arrow + 2);

        add_non_terminal(lhs);
        NonTerminal *nt = find_non_terminal(lhs);
        if (cfg->start_symbol == NULL)
            cfg->start_symbol = nt;

        char *saveptr;
        char *prod = strtok_r(rhs, "|", &saveptr);
        while (prod != NULL) {
            prod = trim(prod);
            Symbol **symbols = NULL;
            int count = 0;
            char *saveptr2;
            char *token = strtok_r(prod, " \t\n", &saveptr2);
            while (token != NULL) {
                Symbol *s = find_terminal(token);
                if (!s) {
                    s = create_symbol(token, true);
                    cfg->terminals = realloc(cfg->terminals, sizeof(Symbol *) * (cfg->terminal_count + 1));
                    cfg->terminals[cfg->terminal_count++] = s;
                }
                symbols = realloc(symbols, sizeof(Symbol *) * (count + 1));
                symbols[count++] = s;
                token = strtok_r(NULL, " \t\n", &saveptr2);
            }
            Production *p = create_production(symbols, count);
            add_production(nt, p);
            prod = strtok_r(NULL, "|", &saveptr);
        }
    }
    fclose(file);
    return cfg;
}

void print_cfg(CFG *cfg) {
    NonTerminal *nt = cfg->non_terminals;
    while (nt) {
        printf("%s -> ", nt->name);
        for (int i = 0; i < nt->production_count; i++) {
            Production *p = nt->productions[i];
            for (int j = 0; j < p->length; j++) {
                printf("%s ", p->symbols[j]->name);
            }
            if (i < nt->production_count - 1)
                printf("| ");
        }
        printf("\n");
        nt = nt->next;
    }
}

// -------------------------
// Left Factoring (Improved)
// -------------------------

// This function groups productions by their first symbol.
// For any group with more than one production, it factors them as:
//    A -> α A'
//    A' -> β1 | β2 | ...   where each β is the suffix (or epsilon if empty)
void left_factor_non_terminal(NonTerminal *nt) {
    if (nt->production_count < 2) return;
    bool *grouped = calloc(nt->production_count, sizeof(bool));

    for (int i = 0; i < nt->production_count; i++) {
        if (grouped[i]) continue;
        Production *prod_i = nt->productions[i];
        if (prod_i->length == 0) continue;
        const char *firstSym = prod_i->symbols[0]->name;
        // Count productions with the same first symbol.
        int groupCount = 0;
        for (int j = i; j < nt->production_count; j++) {
            if (!grouped[j] && nt->productions[j]->length > 0 &&
                strcmp(nt->productions[j]->symbols[0]->name, firstSym) == 0) {
                groupCount++;
            }
        }
        if (groupCount > 1) {
            // Create new non-terminal A_prime
            char newName[MAX_SYMBOL_LEN];
            snprintf(newName, MAX_SYMBOL_LEN, "%s_prime", nt->name);
            add_non_terminal(newName);
            NonTerminal *newNT = find_non_terminal(newName);
            // Process each production in the group.
            for (int j = i; j < nt->production_count; j++) {
                if (!grouped[j] && nt->productions[j]->length > 0 &&
                    strcmp(nt->productions[j]->symbols[0]->name, firstSym) == 0) {
                    Production *p = nt->productions[j];
                    // Suffix is everything after the first symbol.
                    int suffixLen = p->length - 1;
                    Symbol **suffix = NULL;
                    if (suffixLen == 0) {
                        suffix = malloc(sizeof(Symbol *));
                        suffix[0] = create_symbol(EPSILON, true);
                        suffixLen = 1;
                    } else {
                        suffix = malloc(sizeof(Symbol *) * suffixLen);
                        for (int k = 0; k < suffixLen; k++) {
                            suffix[k] = p->symbols[k+1];
                        }
                    }
                    Production *newProd = create_production(suffix, suffixLen);
                    add_production(newNT, newProd);
                    grouped[j] = true;
                }
            }
            // Replace the first production in the group with the factored production.
            Symbol **newSymbols = malloc(sizeof(Symbol *) * 2);
            // Common prefix is the first symbol (e.g., "a" or "b")
            newSymbols[0] = prod_i->symbols[0];
            // Append the new non-terminal symbol.
            Symbol *newSym = create_symbol(newName, false);
            newSymbols[1] = newSym;
            Production *factoredProd = create_production(newSymbols, 2);
            nt->productions[i] = factoredProd;
        }
    }
    free(grouped);

    // Remove productions that were grouped out.
    Production **newProds = NULL;
    int newCount = 0;
    for (int i = 0; i < nt->production_count; i++) {
        // We keep a production if it is non-null and if its length > 0.
        if (nt->productions[i] && nt->productions[i]->length > 0) {
            newProds = realloc(newProds, sizeof(Production *) * (newCount + 1));
            newProds[newCount++] = nt->productions[i];
        }
    }
    free(nt->productions);
    nt->productions = newProds;
    nt->production_count = newCount;
}

void left_factor(CFG *cfg) {
    NonTerminal *nt = cfg->non_terminals;
    while (nt) {
        left_factor_non_terminal(nt);
        nt = nt->next;
    }
}

// -------------------------
// Left Recursion Removal
// -------------------------

void remove_left_recursion_non_terminal(NonTerminal *nt) {
    Production **recursive = NULL;
    int rec_count = 0;
    Production **non_recursive = NULL;
    int nonrec_count = 0;
    
    for (int i = 0; i < nt->production_count; i++) {
        Production *p = nt->productions[i];
        if (p->length > 0 && strcmp(p->symbols[0]->name, nt->name) == 0) {
            recursive = realloc(recursive, sizeof(Production *) * (rec_count + 1));
            recursive[rec_count++] = p;
        } else {
            non_recursive = realloc(non_recursive, sizeof(Production *) * (nonrec_count + 1));
            non_recursive[nonrec_count++] = p;
        }
    }
    
    if (rec_count == 0) {
        free(recursive);
        free(non_recursive);
        return; // no left recursion
    }
    
    // Create new non-terminal A'
    char newName[MAX_SYMBOL_LEN];
    snprintf(newName, MAX_SYMBOL_LEN, "%s_prime", nt->name);
    add_non_terminal(newName);
    NonTerminal *newNT = find_non_terminal(newName);
    
    // For each non-recursive production: A -> β becomes A -> β A'
    for (int i = 0; i < nonrec_count; i++) {
        Production *p = non_recursive[i];
        int newLen = p->length + 1;
        Symbol **newSymbols = malloc(sizeof(Symbol *) * newLen);
        for (int j = 0; j < p->length; j++) {
            newSymbols[j] = p->symbols[j];
        }
        Symbol *newSym = create_symbol(newName, false);
        newSymbols[newLen - 1] = newSym;
        p->symbols = newSymbols;
        p->length = newLen;
    }
    
    // For each recursive production: A -> A α becomes A' -> α A'
    for (int i = 0; i < rec_count; i++) {
        Production *p = recursive[i];
        int newLen = p->length; // skip the recursive A
        if (newLen - 1 <= 0) {
            Symbol **newSymbols = malloc(sizeof(Symbol *));
            newSymbols[0] = create_symbol(EPSILON, true);
            Production *newProd = create_production(newSymbols, 1);
            add_production(newNT, newProd);
        } else {
            Symbol **newSymbols = malloc(sizeof(Symbol *) * (newLen));
            for (int j = 1; j < p->length; j++) {
                newSymbols[j-1] = p->symbols[j];
            }
            Symbol *newSym = create_symbol(newName, false);
            newSymbols[newLen - 1] = newSym;
            Production *newProd = create_production(newSymbols, newLen);
            add_production(newNT, newProd);
        }
    }
    // Also add an epsilon production to newNT.
    Symbol **epsSymbols = malloc(sizeof(Symbol *));
    epsSymbols[0] = create_symbol(EPSILON, true);
    Production *epsProd = create_production(epsSymbols, 1);
    add_production(newNT, epsProd);
    
    // Replace nt->productions with the non-recursive ones.
    free(nt->productions);
    nt->productions = non_recursive;
    nt->production_count = nonrec_count;
    
    free(recursive);
}

void remove_left_recursion(CFG *cfg) {
    NonTerminal *nt = cfg->non_terminals;
    while (nt) {
        remove_left_recursion_non_terminal(nt);
        nt = nt->next;
    }
}

// -------------------------
// FIRST Set Computation
// -------------------------

// Get an array of non-terminals.
NonTerminal **get_non_terminals(CFG *cfg, int *count) {
    int cnt = 0;
    NonTerminal *curr = cfg->non_terminals;
    while (curr) { cnt++; curr = curr->next; }
    NonTerminal **arr = malloc(sizeof(NonTerminal*) * cnt);
    curr = cfg->non_terminals;
    for (int i = 0; i < cnt; i++) {
        arr[i] = curr;
        curr = curr->next;
    }
    *count = cnt;
    return arr;
}

void compute_first_sets(CFG *cfg) {
    int nt_count;
    NonTerminal **nts = get_non_terminals(cfg, &nt_count);
    first_sets = malloc(sizeof(FirstSet) * nt_count);
    
    // Initialize FIRST sets using canonical names.
    for (int i = 0; i < nt_count; i++) {
        strncpy(first_sets[i].name, nts[i]->name, MAX_SYMBOL_LEN);
        first_sets[i].symbols = NULL;
        first_sets[i].count = 0;
    }
    
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < nt_count; i++) {
            NonTerminal *nt = nts[i];
            for (int j = 0; j < nt->production_count; j++) {
                Production *p = nt->productions[j];
                bool allNullable = true;
                for (int k = 0; k < p->length; k++) {
                    Symbol *X = p->symbols[k];
                    if (X->is_terminal) {
                        if (strcmp(X->name, EPSILON) != 0) {
                            if (add_symbol_to_set(X, &first_sets[i].symbols, &first_sets[i].count))
                                changed = true;
                        } else {
                            if (add_symbol_to_set(X, &first_sets[i].symbols, &first_sets[i].count))
                                changed = true;
                        }
                        allNullable = (strcmp(X->name, EPSILON)==0);
                        break;
                    } else {
                        // Find FIRST(X)
                        int idx = -1;
                        for (int m = 0; m < nt_count; m++) {
                            if (strcmp(first_sets[m].name, X->name)==0) {
                                idx = m;
                                break;
                            }
                        }
                        if (idx == -1) break;
                        bool hasEpsilon = false;
                        for (int m = 0; m < first_sets[idx].count; m++) {
                            Symbol *sym = first_sets[idx].symbols[m];
                            if (strcmp(sym->name, EPSILON)==0)
                                hasEpsilon = true;
                            else {
                                if (add_symbol_to_set(sym, &first_sets[i].symbols, &first_sets[i].count))
                                    changed = true;
                            }
                        }
                        if (!hasEpsilon) {
                            allNullable = false;
                            break;
                        }
                    }
                }
                if (allNullable) {
                    Symbol *eps = create_symbol(EPSILON, true);
                    if (add_symbol_to_set(eps, &first_sets[i].symbols, &first_sets[i].count))
                        changed = true;
                }
            }
        }
    } while (changed);
    
    free(nts);
}

void print_first_sets(int nt_count) {
    printf("\nFIRST Sets:\n");
    for (int i = 0; i < nt_count; i++) {
        printf("FIRST(%s) = { ", first_sets[i].name);
        for (int j = 0; j < first_sets[i].count; j++) {
            printf("%s ", first_sets[i].symbols[j]->name);
        }
        printf("}\n");
    }
}

// -------------------------
// FOLLOW Set Computation
// -------------------------

void compute_follow_sets(CFG *cfg) {
    int nt_count;
    NonTerminal **nts = get_non_terminals(cfg, &nt_count);
    follow_sets = malloc(sizeof(FollowSet) * nt_count);
    
    for (int i = 0; i < nt_count; i++) {
        strncpy(follow_sets[i].name, nts[i]->name, MAX_SYMBOL_LEN);
        follow_sets[i].symbols = NULL;
        follow_sets[i].count = 0;
    }
    
    // Add end marker to the start symbol.
    for (int i = 0; i < nt_count; i++) {
        if (strcmp(follow_sets[i].name, cfg->start_symbol->name)==0) {
            Symbol *endSym = create_symbol(END_MARKER, true);
            add_symbol_to_set(endSym, &follow_sets[i].symbols, &follow_sets[i].count);
            break;
        }
    }
    
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < nt_count; i++) {
            NonTerminal *A = nts[i];
            for (int j = 0; j < A->production_count; j++) {
                Production *p = A->productions[j];
                for (int k = 0; k < p->length; k++) {
                    Symbol *B = p->symbols[k];
                    if (!B->is_terminal) {
                        int idxB = -1;
                        for (int m = 0; m < nt_count; m++) {
                            if (strcmp(follow_sets[m].name, B->name)==0) {
                                idxB = m;
                                break;
                            }
                        }
                        if (idxB == -1) continue;
                        
                        bool allNullable = true;
                        for (int l = k+1; l < p->length; l++) {
                            Symbol *X = p->symbols[l];
                            if (X->is_terminal) {
                                if (strcmp(X->name, EPSILON)!=0) {
                                    if (add_symbol_to_set(X, &follow_sets[idxB].symbols, &follow_sets[idxB].count))
                                        changed = true;
                                }
                                allNullable = (strcmp(X->name, EPSILON)==0);
                                break;
                            } else {
                                int idxX = -1;
                                for (int m = 0; m < nt_count; m++) {
                                    if (strcmp(first_sets[m].name, X->name)==0) {
                                        idxX = m;
                                        break;
                                    }
                                }
                                bool hasEpsilon = false;
                                if (idxX != -1) {
                                    for (int m = 0; m < first_sets[idxX].count; m++) {
                                        Symbol *sym = first_sets[idxX].symbols[m];
                                        if (strcmp(sym->name, EPSILON)==0)
                                            hasEpsilon = true;
                                        else {
                                            if (add_symbol_to_set(sym, &follow_sets[idxB].symbols, &follow_sets[idxB].count))
                                                changed = true;
                                        }
                                    }
                                }
                                if (!hasEpsilon) {
                                    allNullable = false;
                                    break;
                                }
                            }
                        }
                        if (allNullable) {
                            int idxA = -1;
                            for (int m = 0; m < nt_count; m++) {
                                if (strcmp(follow_sets[m].name, A->name)==0) {
                                    idxA = m;
                                    break;
                                }
                            }
                            if (idxA != -1) {
                                for (int m = 0; m < follow_sets[idxA].count; m++) {
                                    Symbol *sym = follow_sets[idxA].symbols[m];
                                    if (add_symbol_to_set(sym, &follow_sets[idxB].symbols, &follow_sets[idxB].count))
                                        changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    } while (changed);
    free(nts);
}

void print_follow_sets(int nt_count) {
    printf("\nFOLLOW Sets:\n");
    for (int i = 0; i < nt_count; i++) {
        printf("FOLLOW(%s) = { ", follow_sets[i].name);
        for (int j = 0; j < follow_sets[i].count; j++) {
            printf("%s ", follow_sets[i].symbols[j]->name);
        }
        printf("}\n");
    }
}

// -------------------------
// LL(1) Parsing Table Construction
// -------------------------

typedef struct ParseTable {
    Production ***table; // rows: non-terminals, columns: terminals (plus end marker)
    int nonterminal_count;
    int terminal_count;  // including extra column for '$'
} ParseTable;

ParseTable *build_parse_table(CFG *cfg) {
    int nt_count;
    NonTerminal **nts = get_non_terminals(cfg, &nt_count);
    int term_count = cfg->terminal_count;
    ParseTable *pt = malloc(sizeof(ParseTable));
    pt->nonterminal_count = nt_count;
    pt->terminal_count = term_count + 1; // extra column for END_MARKER

    pt->table = malloc(sizeof(Production**) * nt_count);
    for (int i = 0; i < nt_count; i++) {
        pt->table[i] = calloc(pt->terminal_count, sizeof(Production*));
    }

    // For each non-terminal A and each production A -> α, add production into table
    for (int i = 0; i < nt_count; i++) {
        NonTerminal *A = nts[i];
        // For each production of A
        for (int j = 0; j < A->production_count; j++) {
            Production *p = A->productions[j];
            Symbol **firstAlpha = NULL;
            int firstAlphaCount = 0;
            bool allNullable = true;
            for (int k = 0; k < p->length; k++) {
                Symbol *X = p->symbols[k];
                if (X->is_terminal) {
                    if (strcmp(X->name, EPSILON)!=0)
                        add_symbol_to_set(X, &firstAlpha, &firstAlphaCount);
                    else
                        add_symbol_to_set(X, &firstAlpha, &firstAlphaCount);
                    allNullable = (strcmp(X->name, EPSILON)==0);
                    break;
                } else {
                    int idx = -1;
                    for (int m = 0; m < nt_count; m++) {
                        if (strcmp(first_sets[m].name, X->name)==0) {
                            idx = m;
                            break;
                        }
                    }
                    if (idx != -1) {
                        for (int m = 0; m < first_sets[idx].count; m++) {
                            Symbol *sym = first_sets[idx].symbols[m];
                            if (strcmp(sym->name, EPSILON)!=0)
                                add_symbol_to_set(sym, &firstAlpha, &firstAlphaCount);
                        }
                        bool hasEpsilon = false;
                        for (int m = 0; m < first_sets[idx].count; m++) {
                            if (strcmp(first_sets[idx].symbols[m]->name, EPSILON)==0)
                                hasEpsilon = true;
                        }
                        if (!hasEpsilon) {
                            allNullable = false;
                            break;
                        }
                    }
                }
            }
            // For each terminal in FIRST(α) (excluding epsilon), add production.
            for (int k = 0; k < firstAlphaCount; k++) {
                if (strcmp(firstAlpha[k]->name, EPSILON)!=0) {
                    int tIndex = -1;
                    for (int m = 0; m < cfg->terminal_count; m++) {
                        if (strcmp(cfg->terminals[m]->name, firstAlpha[k]->name)==0) {
                            tIndex = m;
                            break;
                        }
                    }
                    if (tIndex != -1) {
                        pt->table[i][tIndex] = p;
                    }
                }
            }
            // If production can derive epsilon, add production for each terminal in FOLLOW(A).
            bool prodHasEps = false;
            for (int k = 0; k < firstAlphaCount; k++) {
                if (strcmp(firstAlpha[k]->name, EPSILON)==0) {
                    prodHasEps = true;
                    break;
                }
            }
            if (prodHasEps || (p->length==1 && strcmp(p->symbols[0]->name, EPSILON)==0)) {
                int folIdx = -1;
                for (int m = 0; m < nt_count; m++) {
                    if (strcmp(follow_sets[m].name, A->name)==0) {
                        folIdx = m;
                        break;
                    }
                }
                if (folIdx != -1) {
                    for (int m = 0; m < follow_sets[folIdx].count; m++) {
                        Symbol *sym = follow_sets[folIdx].symbols[m];
                        int tIndex = -1;
                        if (strcmp(sym->name, END_MARKER)==0) {
                            tIndex = cfg->terminal_count;
                        } else {
                            for (int n = 0; n < cfg->terminal_count; n++) {
                                if (strcmp(cfg->terminals[n]->name, sym->name)==0) {
                                    tIndex = n;
                                    break;
                                }
                            }
                        }
                        if (tIndex != -1 && tIndex < pt->terminal_count)
                            pt->table[i][tIndex] = p;
                    }
                }
            }
            free(firstAlpha);
        }
    }
    free(nts);
    return pt;
}

void print_parse_table(ParseTable *pt, CFG *cfg) {
    int nt_count = pt->nonterminal_count;
    int t_count = pt->terminal_count;
    
    printf("\nLL(1) Parsing Table:\n");
    printf("%15s", "");
    for (int i = 0; i < cfg->terminal_count; i++) {
        printf("%15s", cfg->terminals[i]->name);
    }
    printf("%15s", END_MARKER);
    printf("\n");
    
    int cnt;
    NonTerminal **nts = get_non_terminals(cfg, &cnt);
    for (int i = 0; i < nt_count; i++) {
        printf("%15s", nts[i]->name);
        for (int j = 0; j < t_count; j++) {
            if (pt->table[i][j] != NULL)
                printf("%15s", "Prod");
            else
                printf("%15s", "");
        }
        printf("\n");
    }
    free(nts);
}

// -------------------------
// MAIN
// -------------------------

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    if (!parse_cfg(argv[1])) {
        printf("Error parsing CFG.\n");
        return 1;
    }
    
    printf("Original CFG:\n");
    print_cfg(cfg);
    
    // Step 1: Left Factoring
    left_factor(cfg);
    printf("\nCFG after Left Factoring:\n");
    print_cfg(cfg);
    
    // Step 2: Left Recursion Removal
    remove_left_recursion(cfg);
    printf("\nCFG after Left Recursion Removal:\n");
    print_cfg(cfg);
    
    // Step 3: FIRST Sets
    int nt_count;
    NonTerminal **nts = get_non_terminals(cfg, &nt_count);
    compute_first_sets(cfg);
    print_first_sets(nt_count);
    
    // Step 4: FOLLOW Sets
    compute_follow_sets(cfg);
    print_follow_sets(nt_count);
    
    // Step 5: LL(1) Parsing Table
    ParseTable *pt = build_parse_table(cfg);
    print_parse_table(pt, cfg);
    
    // Memory cleanup omitted for brevity.
    
    return 0;
}
