/*
* File: codegen.c
* Author: Maya El Zein
* Purpose: This program implements code generation by translating AST nodes into
* three address code and producing MIPS assembly output.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "ast.h"
#include "threeaddr.h"
#include "codegen.h"

static const char *last_callee = NULL;
static QuadList program_code = {NULL, NULL};
static int label_counter = 0;

typedef struct {
    const Symbol *syms[1024];
    int n;
} SymbolSet;

/*
* int newlabel(void) -- returns label_counter increased by 1.
*/
static int newlabel(void) {
    return ++label_counter;
}

/*
* void add_global(SymbolSet *S, const Symbol *p) -- adds a symbol to the given SymbolSet if the
* symbol is valid, if its marked as global, if its not a function, if it has positive size,
* or if it has not already been added.
*/
static void add_global(SymbolSet *S, const Symbol *p) {
    if (!p) {
        return;
    }
    if (!p->is_global) {
        return;
    }
    if (p->is_function) {
        return;
    }
    if (p->size <= 0) {
        return;
    }
    // check if already in the set
    for (int i = 0; i < S->n; i++) {
        if (S->syms[i] == p) {
            return;
        }
    }
    // add symbol if capacity isn't exceeded
    if (S->n < 1024) {
        S->syms[S->n++] = p;
    }
}

/*
* void emit_globals_from_tac(FILE *out, const QuadList *L) -- collects global symbols
* from quads and writes their .data section declarations to output.
*/
static void emit_globals_from_tac(FILE *out, const QuadList *L) {
    SymbolSet G = {0};
    for (const Quad *quad = L->head; quad; quad = quad->next) {
        if (quad->src1.operand_type == OPR_SYM) {
            add_global(&G, quad->src1.val.stptr);
        }
        if (quad->src2.operand_type == OPR_SYM) {
            add_global(&G, quad->src2.val.stptr);
        }
        if (quad->dest.operand_type == OPR_SYM) {
            add_global(&G, quad->dest.val.stptr);
        }
    }
    // no globals found = nothing to output
    if (G.n == 0) {
        return;
    }
    fprintf(out, ".align 2\n.data\n");
    // reserve space for each global variable
    for (int i = 0; i < G.n; i++) {
        int size;
        if (G.syms[i]->size > 0) {
            size = G.syms[i]->size;
        } else {
            size = 4;
        }
        fprintf(out, "_%s: .space %d\n", G.syms[i]->name, size);
    }
    fprintf(out, ".text\n");
}

/*
* Symbol *newtemp(void) -- makes and returns a new temp symbol.
*/
struct Symbol *newtemp(void) {
    struct Symbol *ntmp = calloc(1, sizeof *ntmp);
    char buf[32];
    static int temp_id = 0;
    snprintf(buf, sizeof(buf), "tmp%d", temp_id++);
    ntmp->name = strdup(buf);
    ntmp->is_global = 0;
    ntmp->size = 4;
    ntmp->offset = 0;
    ntmp->is_function = 0;
    ntmp->nformals = 0;
    return ntmp;
}

/*
* Operand make_iconst(int k) -- makes an Operand that holds an
* immediate integer constant.
*/
static Operand make_iconst(int k) {
    Operand o;
    o.operand_type = OPR_ICONST;
    o.val.iconst = k;
    return o;
}

/*
* Operand make_sym(Symbol *p) -- makes an Operand that refers to a Symbol*.
*/
static Operand make_sym(Symbol *p) {
    Operand o;
    o.operand_type = OPR_SYM;
    o.val.stptr = p;
    return o;
}

/*
* Quad *newinstr(OpType op, Operand a, Operand b, Operand d)  -- creates
* a new instruction (quad), fills in its fields and returns the pointer.
*/
Quad *newinstr(OpType op, Operand a, Operand b, Operand d) {
    Quad *ninstr = calloc(1, sizeof *ninstr);
    if (!ninstr) {
        return NULL;
    }
    ninstr->op = op;
    ninstr->src1 = a;
    ninstr->src2 = b;
    ninstr->dest = d;
    return ninstr;
}

/*
* append_quad(QuadList *L, Quad *q)  -- appends Quad q to the end of QuadList L.
*/
void append_quad(QuadList *L, Quad *q) {
    if (!q) {
        return;
    }
    if (!L->head) {
        L->head = L->tail = q;
        return;
    }
    L->tail->next = q;
    L->tail = q;
}

/*
* concat_quadlists(QuadList *L, QuadList *R)  -- concatenates QuadList R
* onto the end of QuadList L.
*/
void concat_quadlists(QuadList *L, QuadList *R) {
    if (!R || !R->head) {
        return;
    }
    if (!L->head) {
        *L = *R;
        return;
    }
    L->tail->next = R->head;
    L->tail = R->tail;
}

typedef struct {
    QuadList code;
    struct Symbol *place;
} Attributes;

/*
* Attributes Ainit(void) -- returns a new, zeroed Attributes.
*/
static Attributes Ainit(void) {
    Attributes a;
    memset(&a, 0, sizeof a);
    return a;
}

/*
* Attributes codeGen_expr(void *E) -- recursively generates quads for expression E.
* Returns an Attributes with code and place.
*/
static Attributes codeGen_expr(void *E) {
    Attributes R = Ainit();
    switch (ast_node_type(E)) {
        case IDENTIFIER: {
            struct Symbol *s = ast_node_symbol(E);
            if (!s) {
                s = calloc(1, sizeof *s);
                s->name = strdup(expr_id_name(E));
                s->is_global = 0;
                s->size = 4;
                s->offset = 0;
                s->is_function = 0;
            }
            R.place = s;
            break;
        }

        case INTCONST: {
            R.place = newtemp();
            append_quad(&R.code, newinstr(OP_ASSG, make_iconst(expr_intconst_val(E)), make_iconst(0), make_sym(R.place)));
            break;
        }

        case UMINUS: {
            Attributes A = codeGen_expr(expr_operand_1(E));
            R.place = newtemp();
            concat_quadlists(&R.code, &A.code);
            append_quad(&R.code, newinstr(OP_NEG, make_sym(A.place), make_iconst(0), make_sym(R.place)));
            break;
        }

        case ADD: case SUB: case MUL: case DIV: {
            Attributes L = codeGen_expr(expr_operand_1(E));
            Attributes Rhs = codeGen_expr(expr_operand_2(E));
            R.place = newtemp();
            concat_quadlists(&R.code, &L.code);
            concat_quadlists(&R.code, &Rhs.code);
            OpType op;
            if (ast_node_type(E) == ADD) {
                op = OP_ADD;
            } else if (ast_node_type(E) == SUB) {
                op = OP_SUB;
            } else if (ast_node_type(E) == MUL) {
                op = OP_MUL;
            } else {
                op = OP_DIV;
            }
            append_quad(&R.code, newinstr(op, make_sym(L.place), make_sym(Rhs.place), make_sym(R.place)));
            break;
        }

        case FUNC_CALL: {
            // push args right to left
            void *args = func_call_args(E);
            int argc = 0;
            // collect in a stack to reverse order
            void *rev = NULL;
            for (void *p = args; p; p = expr_list_rest(p)) {
                // build rev list
                rev = mk_expr_list(expr_list_head(p), rev);
                argc++;
            }
            for (void *p = rev; p; p = expr_list_rest(p)) {
                Attributes Ai = codeGen_expr(expr_list_head(p));
                concat_quadlists(&R.code, &Ai.code);
                append_quad(&R.code, newinstr(OP_PARAM, make_sym(Ai.place), make_iconst(0), make_iconst(0)));
            }

            struct Symbol *f = ast_node_symbol(E);
            if (!f) {
                f = calloc(1, sizeof *f);
                f->name = strdup(func_call_callee(E));
                f->is_function = 1;
                f->is_global = 1;
            }
            append_quad(&R.code, newinstr(OP_CALL, make_sym(f), make_iconst(argc), make_iconst(0)));
            R.place = newtemp();
            append_quad(&R.code, newinstr(OP_RETRIEVE, make_iconst(0), make_iconst(0), make_sym(R.place)));
            break;
        }
        default:
            break;
    }
    return R;
}

/*
* void codeGen_bool(void *B, int Ltrue, int Lfalse, QuadList *out) -- generates
* three-address code for boolean expression B using short-circuit evaluation,
* branching to Ltrue if B is true and Lfalse otherwise.
*/
static void codeGen_bool(void *B, int Ltrue, int Lfalse, QuadList *out) {
    if (!B) {
        return;
    }
    NodeType t = ast_node_type(B);

    // short-circuit AND
    if (t == AND) {
        int Lmid = newlabel();
        void *B1 = expr_operand_1(B);
        void *B2 = expr_operand_2(B);
        codeGen_bool(B1, Lmid, Lfalse, out);
        append_quad(out, newinstr(OP_LABEL, make_iconst(Lmid), make_iconst(0), make_iconst(0)));
        codeGen_bool(B2, Ltrue, Lfalse, out);
        return;
    }

    // short-circuit OR
    if (t == OR) {
        int Lmid = newlabel();
        void *B1 = expr_operand_1(B);
        void *B2 = expr_operand_2(B);
        codeGen_bool(B1, Ltrue, Lmid, out);
        append_quad(out, newinstr(OP_LABEL, make_iconst(Lmid), make_iconst(0), make_iconst(0)));
        codeGen_bool(B2, Ltrue, Lfalse, out);
        return;
    }
    // relational operators (==, !=, <, <=, >, >=)
    Attributes L = codeGen_expr(expr_operand_1(B));
    Attributes R = codeGen_expr(expr_operand_2(B));
    concat_quadlists(out, &L.code);
    concat_quadlists(out, &R.code);
    OpType jump_op;
    switch (t) {
        case EQ:
            jump_op = OP_IF_EQ;
            break;
        case NE:
            jump_op = OP_IF_NE;
            break;
        case LT:
            jump_op = OP_IF_LT;
            break;
        case LE:
            jump_op = OP_IF_LE;
            break;
        case GT:
            jump_op = OP_IF_GT;
            break;
        case GE:
            jump_op = OP_IF_GE;
            break;
        default:
            jump_op = OP_IF_NE;
            break;
    }
    // branch true, else jump false
    append_quad(out, newinstr(jump_op, make_sym(L.place), make_sym(R.place), make_iconst(Ltrue)));
    append_quad(out, newinstr(OP_GOTO, make_iconst(Lfalse), make_iconst(0), make_iconst(0)));
}

/*
* QuadList cg_stmt(void *S) -- generate three-address code for a statement or statement list.
*/
static QuadList cg_stmt(void *S) {
    QuadList L = (QuadList){0};
    if (!S) {
        return L;
    }
    switch (ast_node_type(S)) {
        case STMT_LIST: {
            // iterate through each stmt
            for (void *p = S; p; p = stmt_list_rest(p)) {
                void *s = stmt_list_head(p);
                if (!s) {
                    continue;
                }
                // generate code for this stmt and add to list
                QuadList Li = cg_stmt(s);
                concat_quadlists(&L, &Li);
            }
            break;
        }

        case ASSG: {
            struct Symbol *lhs = ast_node_symbol(S);
            if (!lhs) {
                lhs = calloc(1, sizeof *lhs);
                lhs->name = strdup(stmt_assg_lhs(S));
                lhs->is_global = 0;
                lhs->size = 4;
                lhs->offset = 0;
                lhs->is_function = 0;
            }
            // generate code for rhs
            Attributes rhs = codeGen_expr(stmt_assg_rhs(S));
            concat_quadlists(&L, &rhs.code);
            append_quad(&L, newinstr(OP_ASSG, make_sym(rhs.place), make_iconst(0), make_sym(lhs)));
            break;
        }

        case RETURN: {
            void *rv = stmt_return_expr(S);
            if (rv) {
                // generate code for return value
                Attributes A = codeGen_expr(rv);
                concat_quadlists(&L, &A.code);
                append_quad(&L, newinstr(OP_RETURN, make_sym(A.place), make_iconst(0), make_iconst(0)));
            } else {
                // return w/out value
                append_quad(&L, newinstr(OP_RETURN, make_iconst(0), make_iconst(0), make_iconst(0)));
            }
            break;
        }

        case FUNC_DEF: {
            struct Symbol *f = ast_node_symbol(S);
            if (!f) {
                f = calloc(1, sizeof *f);
                f->name = strdup(func_def_name(S));
                f->is_function = 1; f->is_global = 1;
            }
            append_quad(&L, newinstr(OP_ENTER, make_sym(f), make_iconst(0), make_iconst(0)));
            void *body = func_def_body(S);
            if (body) {
                QuadList B = cg_stmt(body);
                concat_quadlists(&L, &B);
            }
            append_quad(&L, newinstr(OP_LEAVE, make_sym(f), make_iconst(0), make_iconst(0)));
            append_quad(&L, newinstr(OP_RETURN, make_iconst(0), make_iconst(0), make_iconst(0)));
            break;
        }

        case FUNC_CALL: {
            Attributes A = codeGen_expr(S);
            concat_quadlists(&L, &A.code);
            break;
        }

        case IF: {
            int L_then = newlabel();
            int L_else = newlabel();
            int L_after = newlabel();

            void *B  = stmt_if_expr(S);
            void *S1 = stmt_if_then(S);
            void *S2 = stmt_if_else(S);

            if (S2) {
                codeGen_bool(B, L_then, L_else, &L);
            } else {
                codeGen_bool(B, L_then, L_after, &L);
            }

            append_quad(&L, newinstr(OP_LABEL, make_iconst(L_then), make_iconst(0), make_iconst(0)));
            QuadList T1 = cg_stmt(S1);
            concat_quadlists(&L, &T1);

            if (S2) {
                append_quad(&L, newinstr(OP_GOTO, make_iconst(L_after), make_iconst(0), make_iconst(0)));
                append_quad(&L, newinstr(OP_LABEL, make_iconst(L_else), make_iconst(0), make_iconst(0)));
                QuadList T2 = cg_stmt(S2);
                concat_quadlists(&L, &T2);
            }

            append_quad(&L, newinstr(OP_LABEL, make_iconst(L_after), make_iconst(0), make_iconst(0)));
            break;
        }

        case WHILE: {
            int L_top = newlabel();
            int L_body = newlabel();
            int L_after = newlabel();

            append_quad(&L, newinstr(OP_LABEL, make_iconst(L_top), make_iconst(0), make_iconst(0)));
            void *B = stmt_while_expr(S);
            codeGen_bool(B, L_body, L_after, &L);

            append_quad(&L, newinstr(OP_LABEL, make_iconst(L_body), make_iconst(0), make_iconst(0)));
            QuadList SB = cg_stmt(stmt_while_body(S));
            concat_quadlists(&L, &SB);
            append_quad(&L, newinstr(OP_GOTO, make_iconst(L_top), make_iconst(0), make_iconst(0)));
            append_quad(&L, newinstr(OP_LABEL, make_iconst(L_after), make_iconst(0), make_iconst(0)));
            break;
        }
        default:
            break;
    }
    return L;
}

/*
* void codeGen_stmt(void *s) -- generates quads for one statement
* and adds them to the global program list.
*/
void codeGen_stmt(void *s) {
    QuadList L = cg_stmt(s);
    concat_quadlists(&program_code, &L);
}

/*
* void assign_symbol_offset(Symbol *s, int *p_next_local) -- assigns
* a stack offset to a local symbol.
*/
static void assign_symbol_offset(Symbol *s, int *p_next_local) {
    if (!s) {
        return;
    }
    if (s->is_function) {
        return;
    }
    if (s->is_global) {
        return;
    }
    if (s->is_param) {
        if (s->offset == 0) {
            s->offset = 8 + 4 * (s->param_index - 1);
        }
        return;
    }
    // assign next available stack slot for locals
    if (s->offset == 0) {
        s->offset = *p_next_local;
        *p_next_local -= 4;
    }
}

/*
* void enter_frame(FILE *out, int locals_bytes) -- prologue; saves fp/ra, makes space for locals.
*/
static void enter_frame(FILE *out, int locals_bytes) {
    fprintf(out,
        "    la $sp, -8($sp)\n"
        "    sw $fp, 4($sp)\n"
        "    sw $ra, 0($sp)\n"
        "    la $fp, 0($sp)\n"
        "    la $sp, -%d($sp)\n", locals_bytes);
}

/*
* void leave_frame(FILE *out) -- epilogue; restores ra/fp and returns.
*/
static void leave_frame(FILE *out) {
    fprintf(out,
        "    la $sp, 0($fp)\n"
        "    lw $ra, 0($sp)\n"
        "    lw $fp, 4($sp)\n"
        "    la $sp, 8($sp)\n"
        "    jr $ra\n");
}

/*
* void load_symbol(FILE *out, const char *rt, const struct Symbol *s) -- loads a symbol into register rt.
*/
static void load_symbol(FILE *out, const char *rt, const struct Symbol *s) {
    if (s->is_global) {
        fprintf(out, "    lw %s, _%s\n", rt, s->name);
    }
    else {
        fprintf(out, "    lw %s, %d($fp)\n", rt, s->offset);
    }
}

/*
* void store_symbol(FILE *out, const char *rt, const struct Symbol *s) -- stores register rt into a symbol's slot.
*/
static void store_symbol(FILE *out, const char *rt, const struct Symbol *s) {
    if (s->is_global) {
        fprintf(out, "    sw %s, _%s\n", rt, s->name);
    }
    else {
        fprintf(out, "    sw %s, %d($fp)\n", rt, s->offset);
    }
}

/*
* void add_println_runtime(FILE *out) -- adds helper code for println function.
*/
static void add_println_runtime(FILE *out) {
    fprintf(out,
        ".align 2\n"
        ".data\n"
        "_nl: .asciiz \"\\n\"\n\n"
        ".align 2\n"
        ".text\n"
        "_println:\n"
        "    li $v0, 1\n"
        "    lw $a0, 0($sp)\n"
        "    syscall\n"
        "    li $v0, 4\n"
        "    la $a0, _nl\n"
        "    syscall\n"
        "    jr $ra\n\n");
}

/*
* int layout_stack_frame(const Quad *func_entry, const Quad *func_exit) -- assigns stack slots
* for locals. Returns total space needed for locals in this function.
*/
static int layout_stack_frame(const Quad *func_entry, const Quad *func_exit) {
    int next_local = -4;
    const Quad *end = func_exit;

    // scan instructions to find symbols needing stack slots
    for (const Quad *q = func_entry; q && q != end; q = q->next) {
        if (q->src1.operand_type == OPR_SYM) {
            assign_symbol_offset(q->src1.val.stptr, &next_local);
        }
        if (q->src2.operand_type == OPR_SYM) {
            assign_symbol_offset(q->src2.val.stptr, &next_local);
        }
        if (q->dest.operand_type == OPR_SYM) {
            assign_symbol_offset(q->dest.val.stptr, &next_local);
        }
    }
    // space used for locals
    int locals_bytes = (-next_local - 4);
    if (locals_bytes < 0) {
        locals_bytes = 0;
    }
    return locals_bytes;
}

/*
* void assemble_from_tac(FILE *out, const QuadList *L) -- converts three address code into MIPS instructions.
*/
static void assemble_from_tac(FILE *out, const QuadList *L) {
    const Quad *q = L->head;
    while (q) {
        switch (q->op) {
            case OP_ENTER: {
                const Quad *func_end = q->next;
                while (func_end && func_end->op != OP_ENTER) {
                    func_end = func_end->next;
                }
                int locals_bytes = layout_stack_frame(q, func_end);
                fprintf(out, "_%s:\n", q->src1.val.stptr->name);
                enter_frame(out, locals_bytes);
                break;
            }

            case OP_LEAVE:
                break;

            case OP_RETURN: {
                if (q->src1.operand_type == OPR_SYM) {
                    load_symbol(out, "$v0", q->src1.val.stptr);
                } else if (q->src1.operand_type == OPR_ICONST && q->src1.val.iconst != 0) {
                    fprintf(out, "    li $v0, %d\n", q->src1.val.iconst);
                }
                leave_frame(out);
                break;
            }

            case OP_ASSG:
                // assg: dest = src1
                if (q->src1.operand_type == OPR_ICONST) {
                    fprintf(out, "    li $t0, %d\n", q->src1.val.iconst);
                } else {
                    load_symbol(out, "$t0", q->src1.val.stptr);
                }
                store_symbol(out, "$t0", q->dest.val.stptr);
                break;

            case OP_NEG:
                // negate value
                load_symbol(out, "$t0", q->src1.val.stptr);
                fprintf(out, "    sub $t1, $zero, $t0\n");
                store_symbol(out, "$t1", q->dest.val.stptr);
                break;
                
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: {
                // arithmetic ops
                load_symbol(out, "$t0", q->src1.val.stptr);
                load_symbol(out, "$t1", q->src2.val.stptr);
                const char *ins;
                if (q->op == OP_ADD) {
                    ins = "add";
                } else if (q->op == OP_SUB) {
                    ins = "sub";
                } else if (q->op == OP_MUL) {
                    ins = "mul";
                } else {
                    ins = "div";
                }
                fprintf(out, "    %s $t2, $t0, $t1\n", ins);
                store_symbol(out, "$t2", q->dest.val.stptr);
                break;
            }

            case OP_PARAM: {
                // push param onto stack
                if (q->src1.operand_type == OPR_ICONST) {
                    fprintf(out, "    li $t0, %d\n", q->src1.val.iconst);
                } else {
                    load_symbol(out, "$t0", q->src1.val.stptr);
                }
                fprintf(out, "    la $sp, -4($sp)\n");
                fprintf(out, "    sw $t0, 0($sp)\n");
                break;
            }

            case OP_CALL: {
                // call func and adjust stack after args
                last_callee = q->src1.val.stptr->name;
                fprintf(out, "    jal _%s\n", last_callee);
                fprintf(out, "    la $sp, %d($sp)\n", 4 * q->src2.val.iconst);
                break;
            }

            case OP_RETRIEVE: {
                // retrieve return value
                if (!(last_callee && strcmp(last_callee, "println") == 0)) {
                    fprintf(out, "    move $t0, $v0\n");
                    store_symbol(out, "$t0", q->dest.val.stptr);
                }
                break;
            }

            case OP_LABEL: {
                int Lno = q->src1.val.iconst;
                fprintf(out, "__L%d:\n", Lno);
                break;
            }

            case OP_GOTO: {
                int Lno = q->src1.val.iconst;
                fprintf(out, "    j __L%d\n", Lno);
                break;
            }

            case OP_IF_EQ: case OP_IF_NE: case OP_IF_LT: case OP_IF_LE: case OP_IF_GT: case OP_IF_GE: {
                load_symbol(out, "$t0", q->src1.val.stptr);
                load_symbol(out, "$t1", q->src2.val.stptr);
                int Lno = q->dest.val.iconst;

                const char *br = "beq";
                if (q->op == OP_IF_NE) {
                    br = "bne";
                } else if (q->op == OP_IF_LT) {
                    br = "blt";
                } else if (q->op == OP_IF_LE) {
                    br = "ble";
                } else if (q->op == OP_IF_GT) {
                    br = "bgt";
                } else if (q->op == OP_IF_GE) {
                    br = "bge";
                }

                fprintf(out, "    %s $t0, $t1, __L%d\n", br, Lno);
                break;
            }
            default:
                break;
        }
        q = q->next;
    }
}

/*
* void emit_program(FILE *out) -- prints full program.
*/
void emit_program(FILE *out) {
    if (!out) {
        out = stdout;
    }
    emit_globals_from_tac(out, &program_code);
    fprintf(out, ".text\n");
    fprintf(out, ".globl main\n");
    fprintf(out, "main:\n");
    fprintf(out, " j _main\n");
    assemble_from_tac(out, &program_code);
    add_println_runtime(out);
}
