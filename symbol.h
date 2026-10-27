/*
* File: symbol.h
* Author: Maya El Zein
* Purpose: symbol table entry definition
*/

#ifndef SYMBOL_H
#define SYMBOL_H

typedef enum { VAR, FUNC } SymbolKind;

typedef struct Symbol {
    char *name;
    int line;
    SymbolKind kind;
    int nformals;
    struct Symbol *next;
    // codegen fields
    int is_global;
    int offset;
    int size;
    int is_function;
    // mark formals
    int is_param;
    int param_index;
} Symbol;

#endif