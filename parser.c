/*
* File: parser.c
* Author: Maya El Zein
* Purpose: This program is a recursive-descent parser for the C-- language.
* Builds ASTs, supports --chk_decl and --print_ast.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
#include "symbol.h"
#include "ast.h"
#include "codegen.h"

extern int get_token(void);
extern char* lexeme;
extern int line_num;
extern int chk_decl_flag;
extern int print_ast_flag;
extern int gen_code_flag;

// lookahead token
static int curr_tok;
// error tracker
static int error;
// current function's formal parameter count
static int curr_formal_count = 0;
// capacity of dynamically grown array of formal parameters
static int curr_formals_cap = 0;
// array holding formal parameter names
static char **curr_formals_vec = NULL;

typedef struct Scope {
    Symbol *head;
    struct Scope *next;
} Scope;

static Scope *scope_top = NULL;
static Symbol *curr_func_sym = NULL;

/*
 * void push_formal_name(const char *name) -- add a formal parameter name
 * to the current function's dynamic array of formals.
 */
static void push_formal_name(const char *name) {
    // if array is full: double capacity or start with size 8
    if (curr_formal_count >= curr_formals_cap) {
        if (curr_formals_cap) {
            curr_formals_cap = curr_formals_cap * 2;
        } else {
            curr_formals_cap = 8;
        }
        // resize curr_formals_vec to hold curr_formals_cap pointers
        curr_formals_vec = (char**)realloc(curr_formals_vec, curr_formals_cap * sizeof(char*));
    }
    size_t n = strlen(name);
    curr_formals_vec[curr_formal_count] = (char*)malloc(n + 1);
    memcpy(curr_formals_vec[curr_formal_count], name, n + 1);
}

/*
* void scope_stack_setup(void) -- creates the initial global
* scope, is called exactly one time before parsing.
*/
static void scope_stack_setup(void) {
    Scope *global = (Scope *) malloc(sizeof(Scope));
    if (global == NULL) {
        return;
    }
    global->head = NULL;
    global->next = NULL;
    scope_top = global;
}

/*
* void scope_push(void) -- creates the initial global scope.
*/
static void scope_push(void) {
    Scope *new = (Scope *) malloc(sizeof(Scope));
    if (new == NULL) {
        return;
    }
    new->head = NULL;
    new->next = scope_top;
    scope_top = new;
}

/*
* void scope_pop(void) -- pops top symbol table, leaving the
* current scope and freeing all symbols declared in it.
*/
static void scope_pop(void) {
    if (scope_top == NULL) {
        return;
    }
    Symbol *symbol = scope_top->head;
    while (symbol != NULL) {
        Symbol *next_symbol = symbol->next;
        if (!gen_code_flag) {
            free(symbol->name);
            free(symbol);
        }
        symbol = next_symbol;
    }
    Scope *old_top = scope_top;
    scope_top = scope_top->next;
    if (!gen_code_flag) {
        free(old_top);
    }
}

/*
* void declare_in_current_scope(const char *name, SymbolKind kind) -- inserts
* a new symbol into top scope. If semantic checking is on it reject duplicates
* in this scope.
*/
static void declare_in_current_scope(const char *name, SymbolKind kind) {
    if (!chk_decl_flag) {
        return;
    }
    if (!scope_top) {
        return;
    }
    // check for a duplicate in current scope
    for (Symbol *symbol = scope_top->head; symbol; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) {
            fprintf(stderr, "ERROR: '%s' is declared more than once on LINE %d\n", name, line_num);
            error = 1;
            return;
        }
    }
    // allocate and initialize new symbol entry
    Symbol *new_symbol = (Symbol *)malloc(sizeof *new_symbol);
    if (!new_symbol) {
        return;
    }
    size_t s = strlen(name);
    new_symbol->name = (char *)malloc(s + 1);
    if (!new_symbol->name) {
        free(new_symbol);
        return;
    }
    for (size_t i = 0; i <= s; i++) {
        new_symbol->name[i] = name[i];
    }
    new_symbol->line = line_num;
    new_symbol->kind = kind;
    new_symbol->nformals = 0;
    new_symbol->is_function = (kind == FUNC);
    // if in outermost scope = global
    if (scope_top != NULL && scope_top->next == NULL) {
        new_symbol->is_global = 1;
    } else {
        new_symbol->is_global = 0;
    }
    // if variable, size = 4
    if (kind == VAR) {
        new_symbol->size = 4;
    // else function, size = 0
    } else {
        new_symbol->size = 0;
    }
    new_symbol->offset = 0;
    // push symbol onto head of current scope's linked list
    new_symbol->next = scope_top->head;
    scope_top->head = new_symbol;
}

/*
* Symbol *lookup_in_scope(scope, name) -- returns symbol for 'name'
* from the given scope only or NULL if not present.
*/
static Symbol *lookup_in_scope(Scope *scope, const char *name) {
    if (!scope) {
        return NULL;
    }
    for (Symbol *curr = scope->head; curr; curr = curr->next)
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
    return NULL;
}

/*
* Symbol *lookup_visible(name) -- returns the nearest visible symbol for 'name'
* by searching from innermost scope to outermost scope or NULL if not found.
*/
static Symbol *lookup_visible(const char *name) {
    for (Scope *scope = scope_top; scope; scope = scope->next) {
        Symbol *s = lookup_in_scope(scope, name);
        if (s) {
            return s;
        }
    }
    return NULL;
}

static char decl_or_func_id[256];

/*
* void match(int expected) -- checks whether the
* current token matches what is expected.
*/
static void match(int expected) {
    if (curr_tok == expected) {
        curr_tok = get_token();
    } else {
        // error message
        fprintf(stderr, "ERROR: Syntax error on LINE %d\n", line_num);
        error = 1;
    }
}

// Grammar Productions
static void *stmt(void);
static void *opt_stmt_list(void);
static void *while_stmt(void);
static void *if_stmt(void);
static void *return_stmt(void);
static void formals(void);
static void opt_formals(void);
static void id_list(void);
static void var_decl(void);
static void opt_var_decls(void);
static void decl_or_func(void);
static void prog(void);
static void *arith_exp(void);
static void *add_exp(void);
static void *add_exp_tail(void *left);
static void *mul_exp(void);
static void *mul_exp_tail(void *left);
static void *unary_exp(void);
static void *primary(void);
static void *bool_exp(void);
static void *or_exp(void);
static void *or_exp_tail(void *left);
static void *and_exp(void);
static void *and_exp_tail(void *left);
static void *rel_expr(void);
static NodeType relop(void);
static void *expr_list(void);
static void *opt_expr_list(void);

/*
* void type(void) -- type :  kwINT
*/
static void type(void) {
    match(kwINT);
}

/*
* void *stmt(void) -- stmt : fn_call  SEMI  |  while_stmt  |	if_stmt  |	assg_stmt  |  return_stmt
*                                           |  LBRACE  opt_stmt_list  RBRACE  |  SEMI
* returns AST for the statement, NULL for ;.
*/
static void *stmt(void) {
    switch (curr_tok) {
        case kwWHILE:
            return while_stmt();

        case kwIF:
            return if_stmt();

        case kwRETURN:
            return return_stmt();

        case LBRACE: {
            match(LBRACE);
            void *blk = opt_stmt_list();
            match(RBRACE);
            if (error) {
                return NULL;
            }
            return blk;
        }

        case SEMI:
            // ; = empty statement, represented by NULL
            match(SEMI);
            return NULL;

        case ID: {
            // save identifier, consume ID before deciding call vs assignment
            char id_name[256];
            size_t s = strlen(lexeme);
            if (s >= sizeof id_name) {
                s = sizeof id_name - 1;
            }
            memcpy(id_name, lexeme, s);
            id_name[s] = '\0';

            match(ID);
            if (error) {
                return NULL;
            }
            // function call: ID '(' opt_expr_list ')' ';'
            if (curr_tok == LPAREN) {
                Symbol *symbol = NULL;
                if (chk_decl_flag) {
                    symbol = lookup_visible(id_name);
                    if (!symbol) {
                        fprintf(stderr, "ERROR: '%s' is not declared on LINE %d\n", id_name, line_num);
                        error = 1;
                        return NULL;
                    }
                    if (symbol->kind != FUNC) {
                        fprintf(stderr, "ERROR: '%s' is not a function on LINE %d\n", id_name, line_num);
                        error = 1;
                        return NULL;
                    }
                }
                match(LPAREN);
                if (error) {
                    return NULL;
                }
                void *args = opt_expr_list();
                if (error) {
                    return NULL;
                }
                match(RPAREN);
                if (error) {
                    return NULL;
                }
                // check that number of actuals match formals
                if (chk_decl_flag && symbol) {
                    int argc = 0;
                    for (void *p = args; p; p = expr_list_rest(p)) {
                        argc++;
                    }
                    if (argc != symbol->nformals) {
                        fprintf(stderr, "ERROR: wrong # of arguments in call '%s' on LINE %d\n", id_name, line_num);
                        error = 1;
                        return NULL;
                    }
                }

                match(SEMI);
                if (error) {
                    return NULL;
                }
                // build call node
                if (symbol) {
                    return mk_call_sym(id_name, args, symbol);
                } else {
                    return mk_call(id_name, args);
                }
            // assignment: ID '=' arith_exp ';'
            } else if (curr_tok == opASSG) {
                Symbol *symbol = NULL;
                if (chk_decl_flag) {
                    symbol = lookup_visible(id_name);
                    if (!symbol) {
                        fprintf(stderr, "ERROR: '%s' is not declared, LINE %d\n", id_name, line_num);
                        error = 1;
                        return NULL;
                    }
                    if (symbol->kind != VAR) {
                        fprintf(stderr, "ERROR: '%s' is not a variable, LINE %d\n", id_name, line_num);
                        error = 1;
                        return NULL;
                    }
                }
                match(opASSG);
                if (error) {
                    return NULL;
                }

                void *rhs = arith_exp();
                if (error) {
                    return NULL;
                }

                match(SEMI);
                if (error) {
                    return NULL;
                }
                // build assignment node
                if (symbol) {
                    return mk_assg_sym(id_name, rhs, symbol);
                } else {
                    return mk_assg(id_name, rhs);
                }

            } else {
                fprintf(stderr, "ERROR: expected '(' or assignment after identifier, LINE %d\n", line_num);
                error = 1;
                return NULL;
            }
        }

        default:
            fprintf(stderr, "ERROR: unexpected token in statement, LINE %d\n", line_num);
            error = 1;
            return NULL;
    }
}

/*
* void *opt_stmt_list(void) -- opt_stmt_list :  stmt  opt_stmt_list  |  ε
* returns STMT_LIST child0 = head stmt, child1 = rest, else NULL if empty.
*/
static void *opt_stmt_list(void) {
    void *rev = NULL;
    while (curr_tok == ID || curr_tok == kwWHILE || curr_tok == kwIF ||
            curr_tok == kwRETURN || curr_tok == LBRACE || curr_tok == SEMI) {
        void *s = stmt();
        if (error) {
            return rev;
        }
        rev = mk_stmt_list(s, rev);
    }
    // reverse to forward order
    void *frwd = NULL;
    for (void *ptr = rev; ptr; ptr = stmt_list_rest(ptr)) {
        frwd = mk_stmt_list(stmt_list_head(ptr), frwd);
    }
    return frwd;
}

/*
* void formals(void) -- formals	:  type ID  COMMA  formals  |  type ID
*/
static void formals(void) {
    type();
    if (error) {
        return;
    }

    char namebuf[256];
    size_t s = strlen(lexeme);
    if (s >= sizeof namebuf) {
        s = sizeof namebuf - 1;
    }
    memcpy(namebuf, lexeme, s);
    namebuf[s] = '\0';

    declare_in_current_scope(lexeme, VAR);
    if (error) {
        return;
    }
    Symbol *s0 = lookup_in_scope(scope_top, namebuf);
    if (s0) {
        s0->is_param = 1;
        s0->param_index = curr_formal_count + 1;
        s0->is_global = 0;
        s0->size  = 4;
        s0->offset = 0;
    }

    push_formal_name(namebuf);
    curr_formal_count++;

    match(ID);
    if (error) {
        return;
    }

    while (curr_tok == COMMA) {
        match(COMMA);
        if (error) {
            return;
        }

        type();
        if (error) {
            return;
        }

        s = strlen(lexeme);
        if (s >= sizeof namebuf) {
            s = sizeof namebuf - 1;
        }
        memcpy(namebuf, lexeme, s);
        namebuf[s] = '\0';

        declare_in_current_scope(lexeme, VAR);
        if (error) {
            return;
        }
        Symbol *si = lookup_in_scope(scope_top, namebuf);
        if (si) {
            si->is_param = 1;
            si->param_index = curr_formal_count + 1;
            si->is_global = 0;
            si->size = 4;
            si->offset = 0;
        }

        push_formal_name(namebuf);
        curr_formal_count++;

        match(ID);
        if (error) {
            return;
        }
    }
}

/*
* void opt_formals(void) -- opt_formals	:  ε  |  formals
*/
static void opt_formals(void) {
    if (curr_tok == kwINT) {
        formals();
        if (error) {
            return;
        }
    }
}

/*
* void id_list(void) -- id_list	:  ID  |  ID  COMMA  id_list
*/
static void id_list(void) {
    declare_in_current_scope(lexeme, VAR);
    if (error) {
        return;
    }

    match(ID);
    if (error) {
        return;
    }
    while (curr_tok == COMMA) {
        match(COMMA);
        if (error) {
            return;
        }
        declare_in_current_scope(lexeme, VAR);
        if (error) {
            return;
        }
        match(ID);
        if (error) {
            return;
        }
    }
}

/*
* void var_decl(void) -- var_decl :  type  id_list  SEMI
*/
static void var_decl(void) {
    type();
    if (error) {
        return;
    }

    id_list();
    if (error) {
        return;
    }

    match(SEMI);
}

/*
* void opt_var_decls(void) -- opt_var_decls	:  ε  |  var_decl  opt_var_decls
*/
static void opt_var_decls(void) {
    while (curr_tok == kwINT) {
        var_decl();
        if (error) {
            return;
        }
    }
}

/*
* NodeType relop(void) -- relop	:  opEQ  | opNE  |  opLE  |	opLT  |	opGE  |	opGT
* returns NodeType {EQ, NE, LE, LT, GE, GT}.
*/
static NodeType relop(void) {
    // a default
    NodeType nodetype = EQ;
    switch (curr_tok) {
        case opEQ:
            nodetype = EQ;
            break;
        case opNE:
            nodetype = NE;
            break;
        case opLE:
            nodetype = LE;
            break;
        case opLT:
            nodetype = LT;
            break;
        case opGE:
            nodetype = GE;
            break;
        case opGT:
            nodetype = GT;
            break;
        default:
            fprintf(stderr, "ERROR: expected one of '==', '!=', '<=', '<', '>=', '>' on LINE %d\n", line_num);
            error = 1;
            return nodetype;
    }
    curr_tok = get_token();
    return nodetype;
}

/*
* void *expr_list(void) -- expr_list :  arith_exp  COMMA  expr_list  |  arith_exp
* returns EXPR_LIST child0 = head expr, child1 = rest.
*/
static void *expr_list(void) {
    void *rev = NULL;
    void *e = arith_exp();
    if (error) {
        return NULL;
    }

    rev = mk_expr_list(e, rev);
    while (curr_tok == COMMA) {
        match(COMMA);
        if (error) {
            return NULL;
        }
        e = arith_exp();
        if (error) {
            return NULL;
        }
        rev = mk_expr_list(e, rev);
    }
    // reverse to forward
    void *frwd = NULL;
    for (void *ptr = rev; ptr; ptr = expr_list_rest(ptr)) {
        frwd = mk_expr_list(expr_list_head(ptr), frwd);
    }
    return frwd;
}

/*
* int *opt_expr_list(void) -- opt_expr_list :  ε  |  expr_list
* returns NULL if empty, otherwise EXPR_LIST
*/
static void *opt_expr_list(void) {
    if (curr_tok == ID || curr_tok == INTCON ||
        curr_tok == LPAREN || curr_tok == opSUB) {
        return expr_list();
    }
    return NULL;
}

/*
* void *while_stmt(void) -- while_stmt :  kwWHILE  LPAREN  bool_exp  RPAREN  stmt
* returns WHILE node with child0 = condition, child1 = body.
*/
static void *while_stmt(void) {
    match(kwWHILE);
    if (error) {
        return NULL;
    }

    match(LPAREN);
    if (error) {
        return NULL;
    }

    void *cond = bool_exp();
    if (error) {
        return NULL;
    }

    match(RPAREN);
    if (error) {
        return NULL;
    }

    void *body = stmt();
    if (error) {
        return NULL;
    }

    return mk_while(cond, body);
}

/*
* void *if_stmt(void) -- if_stmt :  kwIF  LPAREN  bool_exp  RPAREN  stmt
*                                |  kwIF  LPAREN  bool_exp  RPAREN  stmt  kwELSE  stmt
* returns IF node with child0 = cond, child1 = then, child2 = else, else return NULL.
*/
static void *if_stmt(void) {
    match(kwIF);
    if (error) {
        return NULL;
    }

    match(LPAREN);
    if (error) {
        return NULL;
    }

    void *condition = bool_exp();
    if (error) {
        return NULL;
    }

    match(RPAREN);
    if (error) {
        return NULL;
    }

    void *then_branch = stmt();
    if (error) {
        return NULL;
    }

    void *else_branch = NULL;
    if (curr_tok == kwELSE) {
        match(kwELSE);
        if (error) {
            return NULL;
        }

        else_branch = stmt();
        if (error) {
            return NULL;
        }
    }
    return mk_if(condition, then_branch, else_branch);
}

/*
* void *return_stmt(void) -- return_stmt :  kwRETURN  SEMI
*                                        :  kwRETURN  arith_exp  SEMI
* returns RETURN node with child0 = expr, else NULL for 'return;'.
*/
static void *return_stmt(void) {
    match(kwRETURN);
    if (error) {
        return NULL;
    }

    if (curr_tok == SEMI) {
        match(SEMI);
        return mk_return(NULL);
    }

    void *expr = arith_exp();
    if (error) {
        return NULL;
    }

    match(SEMI);
    if (error) {
        return NULL;
    }
    return mk_return(expr);
}

/*
* void *arith_exp(void) -- arith_exp :  add_exp
*/
static void *arith_exp(void) {
    return add_exp();
}

/*
* void *add_exp(void) -- add_exp :  mul_exp add_exp_tail
*/
static void *add_exp(void) {
    void *left = mul_exp();
    if (error) {
        return NULL;
    }
    return add_exp_tail(left);
}

/*
* void *add_exp_tail(void *left) -- add_exp_tail :  arithop mul_exp add_exp_tail | ε
*/
static void *add_exp_tail(void *left) {
    while (curr_tok == opADD || curr_tok == opSUB) {
        NodeType op;
        if (curr_tok == opADD) {
            op = ADD;
        } else {
            op = SUB;
        }
        match(curr_tok);
        if (error) {
            return NULL;
        }

        void *right = mul_exp();
        if (error) {
            return NULL;
        }

        left = mk_binop(op, left, right);
    }
    return left;
}

/*
* void *mul_exp(void) -- mul_exp :  unary_exp mul_exp_tail
*/
static void *mul_exp(void) {
    void *left = unary_exp();
    if (error) {
        return NULL;
    }
    return mul_exp_tail(left);
}

/*
* void *mul_exp_tail(void *left) -- mul_exp_tail :  opMUL unary_exp mul_exp_tail
*                                                |  opDIV unary_exp mul_exp_tail
*                                                |  ε
*/
static void *mul_exp_tail(void *left) {
    while (curr_tok == opMUL || curr_tok == opDIV) {
        NodeType op;
        if (curr_tok == opMUL) {
            op = MUL;
        } else {
            op = DIV;
        }
        match(curr_tok);
        if (error) {
            return NULL;
        }

        void *right = unary_exp();
        if (error) {
            return NULL;
        }

        left = mk_binop(op, left, right);
    }
    return left;
}

/*
* void *unary_exp(void) -- unary_exp :  opSUB unary_exp
*                                    |  primary
*/
static void *unary_exp(void) {
    if (curr_tok == opSUB) {
        match(opSUB);
        if (error) {
            return NULL;
        }

        void *sub = unary_exp();
        if (error) {
            return NULL;
        }

        return mk_binop(UMINUS, sub, NULL);
    }
    return primary();
}

/*
* void *primary(void) -- primary : ID
*                                | INTCON
*                                | fn_call
*                                | LPAREN arith_exp RPAREN
*/
static void *primary(void) {
    if (curr_tok == INTCON) {
        int v = atoi(lexeme);
        match(INTCON);
        if (error) {
            return NULL;
        }
        return mk_intconst(v);
    }

    if (curr_tok == ID) {
        char namebuf[256];
        size_t s = strlen(lexeme);
        if (s >= sizeof namebuf) {
            s = sizeof namebuf - 1;
        }
        memcpy(namebuf, lexeme, s);
        namebuf[s] = '\0';

        Symbol *sym = NULL;
        if (chk_decl_flag) {
            sym = lookup_visible(namebuf);
            if (!sym) {
                fprintf(stderr, "ERROR: '%s' is not declared on LINE %d\n", namebuf, line_num);
                error = 1;
                return NULL;
            }
        }

        match(ID);
        if (error) {
            return NULL;
        }

        if (curr_tok == LPAREN) {
            if (chk_decl_flag && sym && sym->kind != FUNC) {
                fprintf(stderr, "ERROR: '%s' is not a function on LINE %d\n", namebuf, line_num);
                error = 1;
                return NULL;
            }

            match(LPAREN);
            if (error) {
                return NULL;
            }

            void *args = opt_expr_list();
            if (error) {
                return NULL;
            }

            match(RPAREN);
            if (error) {
                return NULL;
            }

            if (chk_decl_flag && sym) {
                int argc = 0;
                for (void *p = args; p; p = expr_list_rest(p)) {
                    argc++;
                }
                if (argc != sym->nformals) {
                    fprintf(stderr, "ERROR: wrong # of arguments in call '%s' on LINE %d\n", namebuf, line_num);
                    error = 1;
                    return NULL;
                }
            }

            if (sym) {
                return mk_call_sym(namebuf, args, sym);
            }
            return mk_call(namebuf, args);
        }

        if (chk_decl_flag && sym && sym->kind != VAR) {
            fprintf(stderr, "ERROR: '%s' is not a variable on LINE %d\n", namebuf, line_num);
            error = 1;
            return NULL;
        }
        if (sym) {
            return mk_identifier_sym(namebuf, sym);
        }
        return mk_identifier(namebuf);
    }

    if (curr_tok == LPAREN) {
        match(LPAREN);
        if (error) {
            return NULL;
        }

        void *e = arith_exp();
        if (error) {
            return NULL;
        }

        match(RPAREN);
        if (error) {
            return NULL;
        }
        return e;
    }

    fprintf(stderr, "ERROR: expected expression on LINE %d\n", line_num);
    error = 1;
    return NULL;
}

/*
* void *bool_exp(void) -- bool_exp :  or_exp
*/
static void *bool_exp(void) {
    return or_exp();
}

/*
* void *or_exp(void) -- or_exp :  and_exp or_exp_tail
*/
static void *or_exp(void) {
    void *left = and_exp();
    if (error) {
        return NULL;
    }
    return or_exp_tail(left);
}

/*
* void *or_exp_tail(void *left) -- or_exp_tail :  opOR and_exp or_exp_tail | ε
*/
static void *or_exp_tail(void *left) {
    while (curr_tok == opOR) {
        match(opOR);
        if (error) {
            return NULL;
        }

        void *right = and_exp();
        if (error) {
            return NULL;
        }

        left = mk_binop(OR, left, right);
    }
    return left;
}

/*
* void *and_exp(void) -- and_exp :  rel_expr and_exp_tail
*/
static void *and_exp(void) {
    void *left = rel_expr();
    if (error) {
        return NULL;
    }
    return and_exp_tail(left);
}

/*
* void *and_exp_tail(void *left) -- and_exp_tail :  opAND rel_expr and_exp_tail | ε
*/
static void *and_exp_tail(void *left) {
    while (curr_tok == opAND) {
        match(opAND);
        if (error) {
            return NULL;
        }

        void *right = rel_expr();
        if (error) {
            return NULL;
        }

        left = mk_binop(AND, left, right);
    }
    return left;
}

/*
* void *rel_expr(void) -- rel_expr :  arith_exp relop arith_exp
*/
static void *rel_expr(void) {
    void *lhs = arith_exp();
    if (error) {
        return NULL;
    }

    NodeType op = relop();
    if (error) {
        return NULL;
    }

    void *rhs = arith_exp();
    if (error) {
        return NULL;
    }

    return mk_binop(op, lhs, rhs);
}

/*
* void decl_or_func(void) -- disambiguates top-level "type ID ..." as either:
* - a function definition
* - a variable declaration
*/
static void decl_or_func(void) {
    // function definition
    if (curr_tok == LPAREN) {
        declare_in_current_scope(decl_or_func_id, FUNC);
        if (error) {
            return;
        }

        curr_func_sym = lookup_in_scope(scope_top, decl_or_func_id);
        curr_formal_count = 0;

        if (chk_decl_flag) {
            scope_push();
        }

        match(LPAREN);
        if (error) {
            return;
        }

        opt_formals();
        if (error) {
            return;
        }

        match(RPAREN);
        if (error) {
            return;
        }

        if (chk_decl_flag && curr_func_sym) {
            curr_func_sym->nformals = curr_formal_count;
        }

        match(LBRACE);
        opt_var_decls();
        void *body_ast = opt_stmt_list();

        match(RBRACE);

        void *func_ast = mk_func_def(decl_or_func_id, curr_formal_count, curr_formals_vec, body_ast);
        if (gen_code_flag) {
            codeGen_stmt(func_ast);
        }
        if (print_ast_flag) {
            print_ast(func_ast);
        }

        for (int i = 0; i < curr_formal_count; i++) {
            free(curr_formals_vec[i]);
        }
        free(curr_formals_vec);
        curr_formals_vec = NULL;
        curr_formals_cap = 0;
        curr_formal_count = 0;

        if (chk_decl_flag) {
            scope_pop();
        }
    // variable declaration
    } else {
        declare_in_current_scope(decl_or_func_id, VAR);
        if (error) {
            return;
        }
    
        while (curr_tok == COMMA) {
            match(COMMA);
            if (error) {
                return;
            }

            declare_in_current_scope(lexeme, VAR);
            if (error) {
                return;
            }

            match(ID);
            if (error) {
                return;
            }
        }
        match(SEMI);
    }
}

/*
 * void prog(void) -- parses a sequence of top-level declarations
 * prog : func_defn prog | var_decl prog | ε
 */
static void prog(void) {
    while (curr_tok == kwINT) {
        type();
        if (error) {
            return;
        }
        // store ID lexeme so can decide func vs var
        size_t s = strlen(lexeme);
        if (s >= sizeof(decl_or_func_id)) {
            s = sizeof(decl_or_func_id) - 1;
        }
        for (size_t i = 0; i < s; i++) {
            decl_or_func_id[i] = lexeme[i];
        }
        decl_or_func_id[s] = '\0';

        match(ID);
        if (error) {
            return;
        }

        decl_or_func();
        if (error) {
            return;
        }
    }
}

/*
* int parse(void) -- resets the error flag, gets the lookahead token,
* parses prog, and if no error, requires EOF. Returns 0 on success
* and 1 on the first syntax error, after printing error msg.
*/
int parse(void) {
    error = 0;
    scope_stack_setup();
    if (scope_top) {
        Symbol *p = (Symbol*)malloc(sizeof *p);
        memset(p, 0, sizeof *p);
        p->name = strdup("println");
        p->line = 0;
        p->kind = FUNC;
        p->nformals = 1;
        p->is_function = 1;
        p->is_global = 1;
        p->size = 0;
        p->offset = 0;
        p->next = scope_top->head;
        scope_top->head = p;
    }
    curr_tok = get_token();
    prog();
    if (!error) {
        match(EOF);
        if (gen_code_flag) {
            emit_program(stdout);
        }
    }
    return error;
}