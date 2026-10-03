//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "BasicInternal.h"

#define FLOW_FALL   0
#define FLOW_JUMP   1
#define FLOW_END    2
#define FLOW_ERROR  3

static int basicVars[BASIC_MAX_NAMES];

typedef struct {
    int varIndex;
    int limit;
    int step;
    int lineIndex;
    const uint8_t *resume;
} forFrame;

typedef struct {
    int lineIndex;
    const uint8_t *resume;
} gosubFrame;

static forFrame forStack[BASIC_FOR_DEPTH];
static int forDepth;
static gosubFrame gosubStack[BASIC_GOSUB_DEPTH];
static int gosubDepth;

static int currentLine;
static const uint8_t *cursor;
static uint32_t randomState;

void basicVarsReset(void){
    for(int i = 0; i < BASIC_MAX_NAMES; i++){
        basicVars[i] = 0;
    }
}

static void setError(int code){
    basicErr = code;
    if(currentLine >= 0 && currentLine < basicProgCount){
        basicErrLine = basicProg[currentLine].number;
    } else {
        basicErrLine = 0;
    }
}

static int randomNext(void){
    randomState = randomState * 1664525u + 1013904223u;
    return (int)((randomState >> 16) & 0x7FFF);
}

static basicVal makeInt(int intValue){
    basicVal value;
    value.type = BV_INT;
    value.i = intValue;
    value.s = 0;
    return value;
}

static basicVal makeString(char *stringValue){
    basicVal value;
    value.type = BV_STR;
    value.i = 0;
    value.s = stringValue;
    return value;
}

static int formatInteger(int value, char *buffer){
    int length = 0;
    unsigned int magnitude;
    if(value < 0){
        buffer[length++] = '-';
        magnitude = (unsigned int)(-(value + 1)) + 1u;
    } else {
        magnitude = (unsigned int)value;
    }

    char digits[12];
    int digitCount = 0;
    if(magnitude == 0){
        digits[digitCount++] = '0';
    }

    while(magnitude){
        digits[digitCount++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }

    while(digitCount > 0){
        buffer[length++] = digits[--digitCount];
    }
    buffer[length] = 0;
    return length;
}

static void skipToken(void){
    uint8_t token = *cursor;
    if(token == TOK_NUM){
        cursor += 5;
    } else if(token == TOK_STR){
        cursor += 2 + cursor[1];
    } else if(token == TOK_VAR){
        cursor += 2;
    } else {
        cursor += 1;
    }
}

static void skipToEol(void){
    while(*cursor != TOK_EOL){
        skipToken();
    }
}

static basicVal evalExpr(void);

static basicVal evalPrimary(void){
    if(basicErr){
        return makeInt(0);
    }
    uint8_t token = *cursor;

    if(token == TOK_NUM){
        int numberValue = readI32(cursor + 1);
        cursor += 5;
        return makeInt(numberValue);
    }

    if(token == TOK_VAR){
        int varIndex = cursor[1];
        cursor += 2;
        return makeInt(basicVars[varIndex]);
    }

    if(token == TOK_STR){
        int length = cursor[1];
        char *text = basicStrAlloc(length);
        if(!text){
            setError(BASIC_ERR_NOMEM);
            return makeInt(0);
        }

        for(int i = 0; i < length; i++){
            text[i] = (char)cursor[2 + i];
        }

        text[length] = 0;
        cursor += 2 + length;
        return makeString(text);
    }

    if(token == TOK_LPAREN){
        cursor++;
        basicVal value = evalExpr();
        if(*cursor == TOK_RPAREN){
            cursor++;
        } else {
            setError(BASIC_ERR_SYNTAX);
        }
        return value;
    }

    if(token == TOK_FN_TOUCH || token == TOK_FN_TOUCHX || token == TOK_FN_TOUCHY){
        cursor++;
        if(*cursor == TOK_LPAREN){
            cursor++;
            if(*cursor == TOK_RPAREN){
                cursor++;
            }
        }
        if(token == TOK_FN_TOUCH){
            return makeInt(basicInTouch());
        }

        if(token == TOK_FN_TOUCHX){
            return makeInt(basicInTouchX());
        }
        return makeInt(basicInTouchY());
    }

    if(token == TOK_FN_RND || token == TOK_FN_ABS || token == TOK_FN_STR){
        cursor++;
        if(*cursor != TOK_LPAREN){
            setError(BASIC_ERR_SYNTAX);
            return makeInt(0);
        }

        cursor++;
        basicVal argument = evalExpr();
        if(*cursor != TOK_RPAREN){
            setError(BASIC_ERR_SYNTAX);
            return makeInt(0);
        }
        cursor++;
        if(basicErr){
            return makeInt(0);
        }

        if(argument.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }

        if(token == TOK_FN_ABS){
            return makeInt(argument.i < 0 ? -argument.i : argument.i);
        }

        if(token == TOK_FN_RND){
            return makeInt(argument.i > 0 ? (randomNext() % argument.i) : 0);
        }

        char *text = basicStrAlloc(12);
        if(!text){
            setError(BASIC_ERR_NOMEM);
            return makeInt(0);
        }

        formatInteger(argument.i, text);
        return makeString(text);
    }

    if(token == TOK_FN_RGB){
        cursor++;
        if(*cursor != TOK_LPAREN){
            setError(BASIC_ERR_SYNTAX);
            return makeInt(0);
        }

        cursor++;
        basicVal red = evalExpr();
        if(*cursor != TOK_COMMA){
            setError(BASIC_ERR_SYNTAX);
            return makeInt(0);
        }

        cursor++;
        basicVal green = evalExpr();
        if(*cursor != TOK_COMMA){
            setError(BASIC_ERR_SYNTAX);
            return makeInt(0);
        }

        cursor++;
        basicVal blue = evalExpr();
        if(*cursor != TOK_RPAREN){
            setError(BASIC_ERR_SYNTAX);
            return makeInt(0);
        }

        cursor++;
        if(basicErr){
            return makeInt(0);
        }

        if(red.type != BV_INT || green.type != BV_INT || blue.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }

        int color = ((red.i & 0xF8) << 8) | ((green.i & 0xFC) << 3) | (blue.i >> 3);
        return makeInt(color);
    }

    setError(BASIC_ERR_SYNTAX);
    return makeInt(0);
}

static basicVal evalUnary(void){
    if(*cursor == TOK_MINUS){
        cursor++;
        basicVal value = evalUnary();
        if(basicErr){
            return makeInt(0);
        }

        if(value.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }
        return makeInt(-value.i);
    }
    if(*cursor == TOK_NOT){
        cursor++;
        basicVal value = evalUnary();
        if(basicErr){
            return makeInt(0);
        }

        if(value.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }
        return makeInt(value.i ? 0 : 1);
    }
    return evalPrimary();
}

static basicVal evalMul(void){
    basicVal left = evalUnary();
    while(!basicErr && (*cursor == TOK_MUL || *cursor == TOK_DIV || *cursor == TOK_MOD)){
        uint8_t operator = *cursor;
        cursor++;
        basicVal right = evalUnary();
        if(basicErr){
            return makeInt(0);
        }

        if(left.type != BV_INT || right.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }

        if(operator == TOK_MUL){
            left = makeInt(left.i * right.i);
        } else {
            if(right.i == 0){
                setError(BASIC_ERR_DIVZERO);
                return makeInt(0);
            }

            if(operator == TOK_DIV){
                left = makeInt(left.i / right.i);
            } else {
                left = makeInt(left.i % right.i);
            }
        }
    }
    return left;
}

static basicVal evalAdd(void){
    basicVal left = evalMul();
    while(!basicErr && (*cursor == TOK_PLUS || *cursor == TOK_MINUS)){
        uint8_t operator = *cursor;
        cursor++;
        basicVal right = evalMul();
        if(basicErr){
            return makeInt(0);
        }

        if(operator == TOK_PLUS && (left.type == BV_STR || right.type == BV_STR)){
            if(left.type != BV_STR || right.type != BV_STR){
                setError(BASIC_ERR_TYPE);
                return makeInt(0);
            }

            int leftLen = 0;
            int rightLen = 0;
            while(left.s[leftLen]){
                leftLen++;
            }

            while(right.s[rightLen]){
                rightLen++;
            }

            char *joined = basicStrAlloc(leftLen + rightLen);
            if(!joined){
                setError(BASIC_ERR_NOMEM);
                return makeInt(0);
            }

            int writePos = 0;
            for(int i = 0; i < leftLen; i++){
                joined[writePos++] = left.s[i];
            }

            for(int i = 0; i < rightLen; i++){
                joined[writePos++] = right.s[i];
            }

            joined[writePos] = 0;
            left = makeString(joined);
            continue;
        }

        if(left.type != BV_INT || right.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }

        if(operator == TOK_PLUS){
            left = makeInt(left.i + right.i);
        } else {
            left = makeInt(left.i - right.i);
        }
    }
    return left;
}

static basicVal evalCmp(void){
    basicVal left = evalAdd();
    while(!basicErr && (*cursor == TOK_EQ || *cursor == TOK_NE || *cursor == TOK_LT
                     || *cursor == TOK_GT || *cursor == TOK_LE || *cursor == TOK_GE)){
        uint8_t operator = *cursor;
        cursor++;
        basicVal right = evalAdd();
        if(basicErr){
            return makeInt(0);
        }

        if(left.type != BV_INT || right.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }

        int result = 0;
        switch(operator){
            case TOK_EQ: result = left.i == right.i; break;
            case TOK_NE: result = left.i != right.i; break;
            case TOK_LT: result = left.i <  right.i; break;
            case TOK_GT: result = left.i >  right.i; break;
            case TOK_LE: result = left.i <= right.i; break;
            case TOK_GE: result = left.i >= right.i; break;
        }

        left = makeInt(result ? 1 : 0);
    }
    return left;
}

static basicVal evalAnd(void){
    basicVal left = evalCmp();
    while(!basicErr && *cursor == TOK_AND){
        cursor++;
        basicVal right = evalCmp();
        if(basicErr){
            return makeInt(0);
        }

        if(left.type != BV_INT || right.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }

        left = makeInt((left.i && right.i) ? 1 : 0);
    }
    return left;
}

static basicVal evalExpr(void){
    basicVal left = evalAnd();
    while(!basicErr && *cursor == TOK_OR){
        cursor++;
        basicVal right = evalAnd();
        if(basicErr){
            return makeInt(0);
        }

        if(left.type != BV_INT || right.type != BV_INT){
            setError(BASIC_ERR_TYPE);
            return makeInt(0);
        }
        left = makeInt((left.i || right.i) ? 1 : 0);
    }
    return left;
}

static int expectInteger(void){
    basicVal value = evalExpr();
    if(basicErr){
        return 0;
    }
    
    if(value.type != BV_INT){
        setError(BASIC_ERR_TYPE);
        return 0;
    }
    return value.i;
}

static void eatComma(void){
    if(*cursor == TOK_COMMA){
        cursor++;
    } else {
        setError(BASIC_ERR_SYNTAX);
    }
}

static void jumpToLine(int number){
    int lineIndex = basicFindLine(number);
    if(lineIndex < 0){
        setError(BASIC_ERR_NOLINE);
        return;
    }

    currentLine = lineIndex;
    cursor = &basicCode[basicProg[lineIndex].off];
}

static int execStatement(void){
    uint8_t token = *cursor;

    switch(token){
        case TOK_REM: {
            skipToEol();
            return FLOW_FALL;
        }

        case TOK_END: {
            return FLOW_END;
        }

        case TOK_CLS: {
            cursor++;
            int color = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }
            syBasicGFXClear(color);
            return FLOW_FALL;
        }

        case TOK_FILL: {
            cursor++;
            int x = expectInteger();
            eatComma();
            int y = expectInteger();
            eatComma();
            int width = expectInteger();
            eatComma();
            int height = expectInteger();
            eatComma();
            int color = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }
            syBasicGFXFill(x, y, width, height, color);
            return FLOW_FALL;
        }

        case TOK_BOX: {
            cursor++;
            int x = expectInteger();
            eatComma();
            int y = expectInteger();
            eatComma();
            int width = expectInteger();
            eatComma();
            int height = expectInteger();
            eatComma();
            int color = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }
            syBasicGFXBox(x, y, width, height, color);
            return FLOW_FALL;
        }

        case TOK_LINE: {
            cursor++;
            int x1 = expectInteger();
            eatComma();
            int y1 = expectInteger();
            eatComma();
            int x2 = expectInteger();
            eatComma();
            int y2 = expectInteger();
            eatComma();
            int color = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }
            syBasicGFXLine(x1, y1, x2, y2, color);
            return FLOW_FALL;
        }

        case TOK_TEXT: {
            cursor++;
            int x = expectInteger();
            eatComma();
            int y = expectInteger();
            eatComma();
            basicVal textValue = evalExpr();
            eatComma();
            int color = expectInteger();
            eatComma();
            int scale = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }
            if(textValue.type != BV_STR){
                setError(BASIC_ERR_TYPE);
                return FLOW_ERROR;
            }
            syBasicGFXText(x, y, textValue.s, color, scale);
            return FLOW_FALL;
        }

        case TOK_FLUSH: {
            cursor++;
            syBasicGFXFlush();
            return FLOW_FALL;
        }

        case TOK_PRINT: {
            cursor++;
            int suppressNewline = 0;
            while(*cursor != TOK_EOL && *cursor != TOK_COLON){
                basicVal value = evalExpr();
                if(basicErr){
                    return FLOW_ERROR;
                }

                if(value.type == BV_STR){
                    basicOutString(value.s);
                } else {
                    basicOutInt(value.i);
                }

                suppressNewline = 0;
                if(*cursor == TOK_SEMI){
                    cursor++;
                    suppressNewline = 1;
                } else if(*cursor == TOK_COMMA){
                    cursor++;
                    basicOutString(" ");
                    suppressNewline = 1;
                } else {
                    break;
                }
            }

            if(!suppressNewline){
                basicOutNewline();
            }
            return FLOW_FALL;
        }

        case TOK_LET:
        case TOK_VAR: {
            if(token == TOK_LET){
                cursor++;
            }

            if(*cursor != TOK_VAR){
                setError(BASIC_ERR_SYNTAX);
                return FLOW_ERROR;
            }

            int varIndex = cursor[1];
            cursor += 2;
            if(*cursor != TOK_EQ){
                setError(BASIC_ERR_SYNTAX);
                return FLOW_ERROR;
            }

            cursor++;
            int value = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }

            basicVars[varIndex] = value;
            return FLOW_FALL;
        }

        case TOK_GOTO: {
            cursor++;
            int lineNumber = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }

            jumpToLine(lineNumber);
            if(basicErr){
                return FLOW_ERROR;
            }

            return FLOW_JUMP;
        }

        case TOK_GOSUB: {
            cursor++;
            int lineNumber = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }

            if(gosubDepth >= BASIC_GOSUB_DEPTH){
                setError(BASIC_ERR_STACK);
                return FLOW_ERROR;
            }

            gosubStack[gosubDepth].lineIndex = currentLine;
            gosubStack[gosubDepth].resume = cursor;
            gosubDepth++;
            jumpToLine(lineNumber);
            if(basicErr){
                return FLOW_ERROR;
            }

            return FLOW_JUMP;
        }

        case TOK_RETURN: {
            cursor++;
            if(gosubDepth <= 0){
                setError(BASIC_ERR_STACK);
                return FLOW_ERROR;
            }

            gosubDepth--;
            currentLine = gosubStack[gosubDepth].lineIndex;
            cursor = gosubStack[gosubDepth].resume;
            return FLOW_JUMP;
        }

        case TOK_FOR: {
            cursor++;
            if(*cursor != TOK_VAR){
                setError(BASIC_ERR_SYNTAX);
                return FLOW_ERROR;
            }

            int varIndex = cursor[1];
            cursor += 2;
            if(*cursor != TOK_EQ){
                setError(BASIC_ERR_SYNTAX);
                return FLOW_ERROR;
            }
            
            cursor++;
            int startValue = expectInteger();
            if(*cursor != TOK_TO){
                setError(BASIC_ERR_SYNTAX);
                return FLOW_ERROR;
            }

            cursor++;
            int limitValue = expectInteger();
            int stepValue = 1;
            if(*cursor == TOK_STEP){
                cursor++;
                stepValue = expectInteger();
            }

            if(basicErr){
                return FLOW_ERROR;
            }

            if(forDepth >= BASIC_FOR_DEPTH){
                setError(BASIC_ERR_STACK);
                return FLOW_ERROR;
            }

            basicVars[varIndex] = startValue;
            forStack[forDepth].varIndex = varIndex;
            forStack[forDepth].limit = limitValue;
            forStack[forDepth].step = stepValue;
            forStack[forDepth].lineIndex = currentLine;
            forStack[forDepth].resume = cursor;
            forDepth++;
            return FLOW_FALL;
        }

        case TOK_NEXT: {
            cursor++;
            if(*cursor == TOK_VAR){
                cursor += 2;
            }

            if(forDepth <= 0){
                setError(BASIC_ERR_NONEXT);
                return FLOW_ERROR;
            }

            forFrame *frame = &forStack[forDepth - 1];
            basicVars[frame->varIndex] += frame->step;
            int keepGoing;
            if(frame->step >= 0){
                keepGoing = basicVars[frame->varIndex] <= frame->limit;
            } else {
                keepGoing = basicVars[frame->varIndex] >= frame->limit;
            }

            if(keepGoing){
                currentLine = frame->lineIndex;
                cursor = frame->resume;
                return FLOW_JUMP;
            }

            forDepth--;
            return FLOW_FALL;
        }

        case TOK_IF: {
            cursor++;
            int condition = expectInteger();
            if(basicErr){
                return FLOW_ERROR;
            }

            if(*cursor != TOK_THEN){
                setError(BASIC_ERR_SYNTAX);
                return FLOW_ERROR;
            }

            cursor++;
            if(condition){
                if(*cursor == TOK_NUM){
                    int lineNumber = readI32(cursor + 1);
                    cursor += 5;
                    jumpToLine(lineNumber);
                    if(basicErr){
                        return FLOW_ERROR;
                    }
                    return FLOW_JUMP;
                }
                return FLOW_FALL;
            }

            skipToEol();
            return FLOW_FALL;
        }
    }

    setError(BASIC_ERR_SYNTAX);
    return FLOW_ERROR;
}

static int execLine(void){
    while(*cursor != TOK_EOL){
        int flow = execStatement();
        if(flow != FLOW_FALL){
            return flow;
        }

        if(*cursor == TOK_COLON){
            cursor++;
            continue;
        }

        if(*cursor == TOK_EOL){
            break;
        }

        setError(BASIC_ERR_SYNTAX);
        return FLOW_ERROR;
    }
    return FLOW_FALL;
}

int initializeBASICApp(void){
    basicErr = BASIC_OK;
    basicErrLine = 0;
    basicVarsReset();
    forDepth = 0;
    gosubDepth = 0;
    basicStrReset();
    randomState = basicTicks() ^ 0x2545F4A3u;
    
    if(randomState == 0){
        randomState = 1;
    }

    if(basicProgCount == 0){
        return BASIC_OK;
    }

    currentLine = 0;
    cursor = &basicCode[basicProg[0].off];

    for(;;){
        if(currentLine < 0 || currentLine >= basicProgCount){
            break;
        }

        basicStrReset();
        int flow = execLine();
        if(flow == FLOW_ERROR){
            return basicErr;
        }

        if(flow == FLOW_END){
            break;
        }

        basicYield();
        if(flow == FLOW_JUMP){
            continue;
        }

        currentLine++;
        if(currentLine < basicProgCount){
            cursor = &basicCode[basicProg[currentLine].off];
        }
    }
    return BASIC_OK;
}