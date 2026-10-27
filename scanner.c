/*
* File: scanner.c
* Author: Maya El Zein
* Purpose: This program is a scanner for the C-- language.
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "scanner.h"

#define BUFSZ 2048

static char buf[BUFSZ];
char* lexeme = buf;
int lval = 0;

// line number tracker for the parser
int line_num = 1;

/*
* int skip_whitespace_and_comments(void) -- returns 0 if there is
* an unclosed comment, returns 1 if successfully skipped whitespaces
* and comments.
*/
static int skip_whitespace_and_comments(void) {
    int ch;
    int finished = 0;
    
    while (!finished) {
        // skip whitespace
        ch = getchar();
        while (ch != EOF && isspace((unsigned char)ch)) {
            if (ch == '\n') {
                line_num++;
            }
            ch = getchar();
        }
        // check for comments to skip
        if (ch == '/') {
            // look ahead one char
            int next = getchar();
            if (next == '*') {
                int prev = 0;
                int curr = 0;
                int comment_closed = 0;
                // loop through to find end of comment
                while ((curr = getchar()) != EOF) {
                    if (curr == '\n') {
                        line_num++;
                    }
                    // end of comment = comment has been closed correctly
                    if (prev == '*' && curr == '/') {
                        comment_closed = 1;
                        break;
                    }
                    prev = curr;
                }
                // unclosed comment = error return 0
                if (!comment_closed) {
                    strcpy(lexeme, "unclosed comment");
                    return 0;
                }
            // not a comment to skip, push back '/' and lookahead 'next'
            } else {
                if (next != EOF) {
                    ungetc(next, stdin);
                }
                ungetc('/', stdin);
                finished = 1;
            }
        // not a comment, push back ch
        } else {
            if (ch != EOF) {
                ungetc(ch, stdin);
            }
            finished = 1;
        }
    }
    return 1;
}

/*
* int get_token(void) -- reads input from stdin, discards comments
* and whitespace, identifies and returns tokens.
*/
int get_token(void) {
    int ch;

    // will only return 0 if unclosed comment = undefined
    if (!skip_whitespace_and_comments()) {
        return UNDEF;
    }

    ch = getchar();
    // EOF
    if (ch == EOF) {
        lexeme[0] = '\0';
        return EOF;
    }

    // token = LPAREN, left parenthesis ('(')
    if (ch == '(') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return LPAREN;
    }
    // token = RPAREN, right parenthesis (')')
    if (ch == ')') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return RPAREN;
    }
    // token = LBRACE, left curly brace ('{')
    if (ch == '{') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return LBRACE;
    }
    // token = RBRACE, right curly brace ('}')
    if (ch == '}') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return RBRACE;
    }
    // token = COMMA, comma (',')
    if (ch == ',') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return COMMA;
    }
    // token = SEMI, semicolon (';')
    if (ch == ';') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return SEMI;
    }

    // token = INTCON, integer constant (digit { digit })
    if (isdigit((unsigned char)ch)) {
        int i = 0;
        // val = hold int value
        int val = 0;
        // put first digit in lexeme and move i forward
        lexeme[i++] = ch;
        val = ch - '0';
        int next = getchar();
        // while next char is a digit
        while (next != EOF && isdigit((unsigned char)next)) {
            // put next digit in lexeme and move i forward
            if (i < BUFSZ - 1) {
                lexeme[i++] = next;
            }
            // update val
            val = val * 10 + (next - '0');
            // read next char
            next = getchar();
        }
        // put back into input stream
        if (next != EOF) {
            ungetc(next, stdin);
        }
        lexeme[i] = '\0';
        // save int const value
        lval = val;
        return INTCON;
    }

    // token = ID, identifier (	letter { letter | digit | _ })
    if (isalpha((unsigned char)ch)) {
        int i = 0;
        // put first char in lexeme and move i forward
        lexeme[i++] = ch;
        int next = getchar();
        // while next char is a letter or digit or underscore
        while (next != EOF && (isalnum((unsigned char)next) || next == '_')) {
            // put next char in lexeme and move i forward
            if (i < BUFSZ - 1) {
                lexeme[i++] = next;
            }
            // read next char
            next = getchar();
        }
        // put back into input stream
        if (next != EOF) {
            ungetc(next, stdin);
        }
        lexeme[i] = '\0';
        // token = kwINT, (int)
        if (strcmp(lexeme, "int") == 0) {
            return kwINT;
        // token = kwIF, (if)
        } else if (strcmp(lexeme, "if") == 0) {
            return kwIF;
        // token = kwELSE, (else)
        } else if (strcmp(lexeme, "else") == 0) {
            return kwELSE;
        // token = kwWHILE, (while)
        } else if (strcmp(lexeme, "while") == 0) {
            return kwWHILE;
        // token = kwRETURN, (return)
        } else if (strcmp(lexeme, "return") == 0) {
            return kwRETURN;
        }
        return ID;
    }

    // Operations
    // peek at next char for each
    if (ch == '<') {
        int next = getchar();
        // token = opLE, less than or equal to (<=)
        if (next == '=') {
            lexeme[0] = '<';
            lexeme[1] = '=';
            lexeme[2] = '\0';
            return opLE;
        // token = opLT, less than (<)
        } else {
            // put back into input stream
            if (next != EOF) {
                ungetc(next, stdin);
            }
            lexeme[0] = '<';
            lexeme[1] = '\0';
            return opLT;
        }
    }
    if (ch == '>') {
        int next = getchar();
         // token = opGE, greater than or equal to (>=)
        if (next == '=') {
            lexeme[0] = '>';
            lexeme[1] = '=';
            lexeme[2] = '\0';
            return opGE;
        // token = opGT, greater than (>)
        } else {
            // put back into input stream
            if (next != EOF) {
                ungetc(next, stdin);
            }
            lexeme[0] = '>';
            lexeme[1] = '\0';
            return opGT;
        }
    }
    if (ch == '=') {
        int next = getchar();
        // token = opEQ, equals to (==)
        if (next == '=') {
            lexeme[0] = '=';
            lexeme[1] = '=';
            lexeme[2] = '\0';
            return opEQ;
        // token = opASSG, equals assignment (=)
        } else {
            // put back into input stream
            if (next != EOF) {
                ungetc(next, stdin);
            }
            lexeme[0] = '=';
            lexeme[1] = '\0';
            return opASSG;
        }
    }
    if (ch == '!') {
        int next = getchar();
        // token = opNE, not equal to (!=)
        if (next == '=') {
            lexeme[0] = '!';
            lexeme[1] = '=';
            lexeme[2] = '\0';
            return opNE;
        // token = opNOT, not logical operator (!)
        } else {
            // put back into input stream
            if (next != EOF) {
                ungetc(next, stdin);
            }
            lexeme[0] = '!';
            lexeme[1] = '\0';
            return opNOT;
        }
    }
    if (ch == '&') {
        int next = getchar();
        // token = opAND, and logical operator (&&)
        if (next == '&') {
            lexeme[0] = '&';
            lexeme[1] = '&';
            lexeme[2] = '\0';
            return opAND;
        // token = UNDEF, undefined (&)
        } else {
            // put back into input stream
            if (next != EOF) {
                ungetc(next, stdin);
            }
            lexeme[0] = '&';
            lexeme[1] = '\0';
            return UNDEF;
        }
    }
    if (ch == '|') {
        int next = getchar();
        // token = opOR, or logical operator (||)
        if (next == '|') {
            lexeme[0] = '|';
            lexeme[1] = '|';
            lexeme[2] = '\0';
            return opOR;
        // token = UNDEF, undefined (|)
        } else {
            // put back into input stream
            if (next != EOF) {
                ungetc(next, stdin);
            }
            lexeme[0] = '|';
            lexeme[1] = '\0';
            return UNDEF;
        }
    }

    // Operations continued
    // token = opADD, addition (+)
    if (ch == '+') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return opADD;
    }
    // token = opSUB, subtraction (-)
    if (ch == '-') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return opSUB;
    }
    // token = opMUL, multiplication (*)
    if (ch == '*') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return opMUL;
    }
    // token = opDIV, division (/)
    if (ch == '/') {
        lexeme[0] = ch;
        lexeme[1] = '\0';
        return opDIV;
    }

    // lastly, if nothing else it's undefined
    lexeme[0] = ch;
    lexeme[1] = '\0';
    return UNDEF;
}