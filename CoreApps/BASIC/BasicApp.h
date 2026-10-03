//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#ifndef BASIC_H
#define BASIC_H

#include "Kernel.h"

#define BASIC_MAX_LINES    512
#define BASIC_MAX_NAMES    96
#define BASIC_ARENA_BYTES  (40 * 1024)
#define BASIC_STR_MAX      64
#define BASIC_GOSUB_DEPTH  24
#define BASIC_FOR_DEPTH    16

enum {
    TOK_EOL = 0,
    TOK_NUM,
    TOK_STR,
    TOK_VAR,
    TOK_PRINT,
    TOK_LET,
    TOK_IF,
    TOK_THEN,
    TOK_GOTO,
    TOK_GOSUB,
    TOK_RETURN,
    TOK_FOR,
    TOK_TO,
    TOK_STEP,
    TOK_NEXT,
    TOK_REM,
    TOK_END,
    TOK_CLS,
    TOK_BOX,
    TOK_FILL,
    TOK_TEXT,
    TOK_LINE,
    TOK_FLUSH,
    TOK_FN_TOUCH,
    TOK_FN_TOUCHX,
    TOK_FN_TOUCHY,
    TOK_FN_RND,
    TOK_FN_ABS,
    TOK_FN_STR,
    TOK_FN_RGB,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_MOD,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_SEMI,
    TOK_COLON,
    TOK_ASSIGN
};

enum {
    BV_INT = 0,
    BV_STR = 1
};

typedef struct {
    int type;
    int i;
    char *s;
} basicVal;

typedef struct {
    int number;
    int off;
    int len;
} basicLine;

enum {
    BASIC_OK = 0,
    BASIC_ERR_SYNTAX,
    BASIC_ERR_NOLINE,
    BASIC_ERR_TYPE,
    BASIC_ERR_DIVZERO,
    BASIC_ERR_NOMEM,
    BASIC_ERR_STACK,
    BASIC_ERR_NONEXT,
    BASIC_ERR_BREAK
};

void basicReset(void);
int  basicEnterLine(const char *text);
int  initializeBASICApp(void);
int  basicLastError(void);
int  basicErrorLine(void);
const char *basicErrorText(int err);
void syAppHubInit(void);

#endif