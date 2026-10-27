/*
* File: threeaddr.h
* Author: Maya El Zein
* Purpose: simple three-address (quad) IR and its API for code generation
*/

#ifndef THREEADDR_H
#define THREEADDR_H

struct Symbol;
typedef struct Symbol Symbol;

typedef enum { OPR_ICONST, OPR_SYM } OperandType;

typedef struct {
    OperandType operand_type;
    union {
        int iconst;
        Symbol *stptr;
    } val;
} Operand;

typedef enum {
    OP_ENTER, OP_LEAVE, OP_RETURN, OP_ASSG, OP_NEG, OP_ADD,
    OP_SUB, OP_MUL, OP_DIV, OP_PARAM, OP_CALL, OP_RETRIEVE,
    OP_GOTO, OP_LABEL,
    OP_IF_EQ, OP_IF_NE, OP_IF_LT, OP_IF_LE, OP_IF_GT, OP_IF_GE
} OpType;

typedef struct quad {
    OpType op;
    Operand src1;
    Operand src2;
    Operand dest;
    struct quad *next;
} Quad;

typedef struct quad_list {
    Quad *head, *tail;
} QuadList;

Symbol *newtemp(void);
Quad *newinstr(OpType op, Operand a, Operand b, Operand d);
void append_quad(QuadList *L, Quad *q);
void concat_quadlists(QuadList *L, QuadList *R);

#endif