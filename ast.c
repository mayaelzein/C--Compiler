/*
* File: ast.c
* Author: Maya El Zein
* Purpose: This program implements the AST for G2.
*/

#include <stdlib.h>
#include <string.h>
#include "ast.h"

typedef struct ast_node {
    NodeType ntype;
    struct Symbol *st_ref;
    int num;
    struct ast_node *child0;
    struct ast_node *child1;
    struct ast_node *child2;
    char *id;
    int nargs;
    char **argnames;
} ASTnode;

static char *ast_strdup(const char *s) {
    size_t n = strlen(s);
    char *copy = (char*)malloc(n+1);
    if (copy) {
        memcpy(copy, s, n+1);
    }
    return copy;
}

static ASTnode *ast_new_node(NodeType t) {
    ASTnode *p = calloc(1, sizeof *p);
    if (p == NULL) {
        return NULL;
    }
    p->ntype = t;
    return p;
}

struct Symbol *ast_node_symbol(void *ptr) {
    if (ptr == NULL) {
        return NULL;
    }
    return ((ASTnode *)ptr)->st_ref;
}

void ast_set_symbol(void *ptr, struct Symbol *sym){
    if (ptr) {
        ((ASTnode*)ptr)->st_ref = sym;
    }
}

void *mk_stmt_list(void *head, void *rest) {
    ASTnode *p = ast_new_node(STMT_LIST);
    p->child0 = head;
    p->child1 = rest;
    return p;
}

void *mk_expr_list(void *head, void *rest) {
    ASTnode *p = ast_new_node(EXPR_LIST);
    p->child0 = head;
    p->child1 = rest;
    return p;
}

void *mk_identifier(const char *name) {
    ASTnode *p = ast_new_node(IDENTIFIER);
    p->id  = ast_strdup(name);
    return p;
}
void *mk_identifier_sym(const char *name, struct Symbol *sym) {
    ASTnode *p = (ASTnode*)mk_identifier(name);
    p->st_ref = sym;
    return p;
}
void *mk_intconst(int v) {
    ASTnode *p = ast_new_node(INTCONST);
    p->num = v;
    return p;
}

void *mk_binop(NodeType op, void *lhs, void *rhs) {
    ASTnode *p = ast_new_node(op);
    p->child0 = lhs;
    p->child1 = rhs;
    return p;
}

void *mk_assg(const char *lhs_name, void *rhs_expr) {
    ASTnode *p = ast_new_node(ASSG);
    p->id = ast_strdup(lhs_name);
    p->child0 = rhs_expr;
    return p;
}
void *mk_assg_sym(const char *lhs_name, void *rhs_expr, struct Symbol *lhs_sym) {
    ASTnode *p = (ASTnode*)mk_assg(lhs_name, rhs_expr);
    p->st_ref = lhs_sym;
    return p;
}

void *mk_if(void *cond, void *thenp, void *elsep) {
    ASTnode *p = ast_new_node(IF);
    p->child0 = cond;
    p->child1 = thenp;
    p->child2 = elsep;
    return p;
}
void *mk_while(void *cond, void *body) {
    ASTnode *p = ast_new_node(WHILE);
    p->child0 = cond;
    p->child1 = body;
    return p;
}
void *mk_return(void *expr_opt) {
    ASTnode *p = ast_new_node(RETURN);
    p->child0 = expr_opt;
    return p;
}

void *mk_call(const char *callee, void *args_opt) {
    ASTnode *p = ast_new_node(FUNC_CALL);
    p->id = ast_strdup(callee);
    p->child0 = args_opt;
    return p;
}
void *mk_call_sym(const char *callee, void *args_opt, struct Symbol *callee_sym) {
    ASTnode *p = (ASTnode*)mk_call(callee, args_opt);
    p->st_ref = callee_sym;
    return p;
}

void *mk_func_def(const char *name, int nargs, char **argnames, void *body) {
    ASTnode *p = ast_new_node(FUNC_DEF);
    p->id = ast_strdup(name);
    p->nargs = nargs;
    p->argnames = argnames;
    p->child0 = body;
    return p;
}

NodeType ast_node_type(void *ptr) {
    return ((ASTnode*)ptr)->ntype;
}

char *func_def_name(void *ptr) {
    return ((ASTnode*)ptr)->id;
}

int func_def_nargs(void *ptr) {
    return ((ASTnode*)ptr)->nargs;
}

void *func_def_body(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

char *func_def_argname(void *ptr, int n) {
    ASTnode *a = (ASTnode*)ptr;
    if (n <= 0 || n > a->nargs) {
        return NULL;
    }
    return a->argnames[n-1];
}

char *func_call_callee(void *ptr) {
    return ((ASTnode*)ptr)->id;
}

void *func_call_args(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

void *stmt_list_head(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

void *stmt_list_rest(void *ptr) {
    return ((ASTnode*)ptr)->child1;
}

void *expr_list_head(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

void *expr_list_rest(void *ptr) {
    return ((ASTnode*)ptr)->child1;
}

char *expr_id_name(void *ptr) {
    return ((ASTnode*)ptr)->id;
}

int expr_intconst_val(void *ptr) {
    return ((ASTnode*)ptr)->num;
}

void *expr_operand_1(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

void *expr_operand_2(void *ptr) {
    return ((ASTnode*)ptr)->child1;
}

void *stmt_if_expr(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

void *stmt_if_then(void *ptr) {
    return ((ASTnode*)ptr)->child1;
}

void *stmt_if_else(void *ptr) {
    return ((ASTnode*)ptr)->child2;
}

char *stmt_assg_lhs(void *ptr) {
    return ((ASTnode*)ptr)->id;
}

void *stmt_assg_rhs(void *ptr) {
    return ((ASTnode*)ptr)->child0;
}

void *stmt_while_expr(void *ptr){
    return ((ASTnode*)ptr)->child0;
}

void *stmt_while_body(void *ptr){
    return ((ASTnode*)ptr)->child1;
}

void *stmt_return_expr(void *ptr){
    return ((ASTnode*)ptr)->child0;
}