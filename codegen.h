/*
* File: codegen.h
* Author: Maya El Zein
* Purpose: function declarations for generating intermediate code
* from statements and printing the program output
*/

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

void codeGen_stmt(void *S);
void emit_program(FILE *out);

#endif
