//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "BasicInternal.h"

uint8_t  basicCode[BASIC_CODE_BYTES];
int      basicCodeTop;
basicLine basicProg[BASIC_MAX_LINES];
int      basicProgCount;

char basicNames[BASIC_MAX_NAMES][BASIC_NAME_LEN];
int  basicNameCount;

int basicErr;
int basicErrLine;

static char stringScratch[BASIC_STR_SCRATCH];
static int  stringScratchTop;

int readI32(const uint8_t *bytes){
    return (int)((uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24));
}

void writeI32(uint8_t *bytes, int value){
    uint32_t raw = (uint32_t)value;
    bytes[0] = (uint8_t)(raw);
    bytes[1] = (uint8_t)(raw >> 8);
    bytes[2] = (uint8_t)(raw >> 16);
    bytes[3] = (uint8_t)(raw >> 24);
}

char *basicStrAlloc(int length){
    if(length < 0){
        return 0;
    }

    if(stringScratchTop + length + 1 > BASIC_STR_SCRATCH){
        return 0;
    }

    char *result = &stringScratch[stringScratchTop];
    stringScratchTop += length + 1;
    return result;
}

void basicStrReset(void){
    stringScratchTop = 0;
}

static int toUpperChar(int ch){
    if(ch >= 'a' && ch <= 'z'){
        return ch - 32;
    }
    return ch;
}

static int isAlphaChar(int ch){
    ch = toUpperChar(ch);
    return ch >= 'A' && ch <= 'Z';
}

static int isDigitChar(int ch){
    return ch >= '0' && ch <= '9';
}

static int wordEquals(const char *text, int textLen, const char *word){
    int position = 0;
    while(word[position]){
        if(position >= textLen){
            return 0;
        }

        if(toUpperChar((unsigned char)text[position]) != (unsigned char)word[position]){
            return 0;
        }
        position++;
    }
    return position == textLen;
}

static const char *keywordText[] = {
    "PRINT", "LET", "IF", "THEN", "GOTO", "GOSUB", "RETURN", "FOR", "TO", "STEP", "NEXT", "REM", "END", "CLS", "BOX", "FILL", "TEXT", "LINE", "FLUSH", "MOD", "AND", "OR", "NOT",
    "TOUCHX", "TOUCHY", "TOUCH", "RND", "ABS", "STR$", "RGB",
    0
};

static const int keywordTokens[] = {
    TOK_PRINT, TOK_LET, TOK_IF, TOK_THEN, TOK_GOTO, TOK_GOSUB, TOK_RETURN, TOK_FOR, TOK_TO, TOK_STEP, TOK_NEXT, TOK_REM, TOK_END, TOK_CLS, TOK_BOX, TOK_FILL, TOK_TEXT, TOK_LINE, TOK_FLUSH,
    TOK_MOD, TOK_AND, TOK_OR, TOK_NOT,
    TOK_FN_TOUCHX, TOK_FN_TOUCHY, TOK_FN_TOUCH, TOK_FN_RND, TOK_FN_ABS, TOK_FN_STR, TOK_FN_RGB
};

static int keywordToken(const char *text, int length){
    for(int i = 0; keywordText[i]; i++){
        if(wordEquals(text, length, keywordText[i])){
            return keywordTokens[i];
        }
    }
    return -1;
}

int basicNameIndex(const char *text, int length){
    if(length >= BASIC_NAME_LEN){
        length = BASIC_NAME_LEN - 1;
    }

    for(int nameIndex = 0; nameIndex < basicNameCount; nameIndex++){
        int position = 0;
        while(position < length && basicNames[nameIndex][position]){
            if(toUpperChar((unsigned char)text[position]) != (unsigned char)basicNames[nameIndex][position]){
                break;
            }
            position++;
        }

        if(position == length && basicNames[nameIndex][position] == 0){
            return nameIndex;
        }
    }

    if(basicNameCount >= BASIC_MAX_NAMES){
        return -1;
    }

    int newIndex = basicNameCount++;
    int writePos = 0;

    while(writePos < length){
        basicNames[newIndex][writePos] = (char)toUpperChar((unsigned char)text[writePos]);
        writePos++;
    }

    basicNames[newIndex][writePos] = 0;
    return newIndex;
}

int basicFindLine(int number){
    int low = 0;
    int high = basicProgCount - 1;
    while(low <= high){
        int middle = (low + high) / 2;
        if(basicProg[middle].number == number){
            return middle;
        }

        if(basicProg[middle].number < number){
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return -1;
}

static int tokenizeBody(const char *text, uint8_t *tokens, int capacity){
    int outLen = 0;

    while(*text){
        while(*text == ' ' || *text == '\t'){
            text++;
        }

        if(*text == 0){
            break;
        }

        if(outLen + 6 >= capacity){
            return -1;
        }

        char ch = *text;

        if(isDigitChar((unsigned char)ch)){
            int numberValue = 0;
            while(isDigitChar((unsigned char)*text)){
                numberValue = numberValue * 10 + (*text - '0');
                text++;
            }
            tokens[outLen++] = TOK_NUM;
            writeI32(&tokens[outLen], numberValue);
            outLen += 4;
            continue;
        }

        if(ch == '"'){
            text++;
            tokens[outLen++] = TOK_STR;
            int lengthPos = outLen++;
            int stringLen = 0;
            while(*text && *text != '"'){
                if(stringLen >= BASIC_STR_MAX - 1){
                    text++;
                    continue;
                }
                if(outLen + 1 >= capacity){
                    return -1;
                }
                tokens[outLen++] = (uint8_t)*text++;
                stringLen++;
            }
            if(*text == '"'){
                text++;
            }
            tokens[lengthPos] = (uint8_t)stringLen;
            continue;
        }

        if(isAlphaChar((unsigned char)ch)){
            const char *identStart = text;
            while(isAlphaChar((unsigned char)*text) || isDigitChar((unsigned char)*text)){
                text++;
            }
            if(*text == '$'){
                text++;
            }
            int identLen = (int)(text - identStart);
            int keywordId = keywordToken(identStart, identLen);
            if(keywordId == TOK_REM){
                tokens[outLen++] = TOK_REM;
                while(*text){
                    text++;
                }
                continue;
            }
            if(keywordId >= 0){
                tokens[outLen++] = (uint8_t)keywordId;
                continue;
            }
            int nameIndex = basicNameIndex(identStart, identLen);
            if(nameIndex < 0){
                return -2;
            }
            tokens[outLen++] = TOK_VAR;
            tokens[outLen++] = (uint8_t)nameIndex;
            continue;
        }

        if(ch == '<'){
            if(text[1] == '>'){
                tokens[outLen++] = TOK_NE;
                text += 2;
                continue;
            }
            if(text[1] == '='){
                tokens[outLen++] = TOK_LE;
                text += 2;
                continue;
            }
            tokens[outLen++] = TOK_LT;
            text++;
            continue;
        }
        if(ch == '>'){
            if(text[1] == '='){
                tokens[outLen++] = TOK_GE;
                text += 2;
                continue;
            }
            tokens[outLen++] = TOK_GT;
            text++;
            continue;
        }

        int punctToken = -1;
        switch(ch){
            case '+': punctToken = TOK_PLUS;   break;
            case '-': punctToken = TOK_MINUS;  break;
            case '*': punctToken = TOK_MUL;    break;
            case '/': punctToken = TOK_DIV;    break;
            case '=': punctToken = TOK_EQ;     break;
            case '(': punctToken = TOK_LPAREN; break;
            case ')': punctToken = TOK_RPAREN; break;
            case ',': punctToken = TOK_COMMA;  break;
            case ';': punctToken = TOK_SEMI;   break;
            case ':': punctToken = TOK_COLON;  break;
            default:  punctToken = -1;         break;
        }
        if(punctToken < 0){
            return -2;
        }
        tokens[outLen++] = (uint8_t)punctToken;
        text++;
    }

    tokens[outLen++] = TOK_EOL;
    return outLen;
}

static void removeLineAt(int lineIndex){
    for(int i = lineIndex; i < basicProgCount - 1; i++){
        basicProg[i] = basicProg[i + 1];
    }
    basicProgCount--;
}

static int storeLine(int number, const uint8_t *tokens, int length){
    if(basicCodeTop + length > BASIC_CODE_BYTES){
        return BASIC_ERR_NOMEM;
    }

    int codeOffset = basicCodeTop;
    for(int i = 0; i < length; i++){
        basicCode[codeOffset + i] = tokens[i];
    }

    basicCodeTop += length;

    int existingIndex = basicFindLine(number);
    if(existingIndex >= 0){
        basicProg[existingIndex].off = codeOffset;
        basicProg[existingIndex].len = length;
        return BASIC_OK;
    }

    if(basicProgCount >= BASIC_MAX_LINES){
        return BASIC_ERR_NOMEM;
    }

    int insertPos = 0;
    while(insertPos < basicProgCount && basicProg[insertPos].number < number){
        insertPos++;
    }

    for(int i = basicProgCount; i > insertPos; i--){
        basicProg[i] = basicProg[i - 1];
    }

    basicProg[insertPos].number = number;
    basicProg[insertPos].off = codeOffset;
    basicProg[insertPos].len = length;
    basicProgCount++;
    return BASIC_OK;
}

int basicEnterLine(const char *text){
    const char *reader = text;
    while(*reader == ' ' || *reader == '\t'){
        reader++;
    }
    if(!isDigitChar((unsigned char)*reader)){
        basicErr = BASIC_ERR_SYNTAX;
        return basicErr;
    }
    int number = 0;
    while(isDigitChar((unsigned char)*reader)){
        number = number * 10 + (*reader - '0');
        reader++;
    }
    while(*reader == ' ' || *reader == '\t'){
        reader++;
    }

    if(*reader == 0){
        int existingIndex = basicFindLine(number);
        if(existingIndex >= 0){
            removeLineAt(existingIndex);
        }
        basicErr = BASIC_OK;
        return basicErr;
    }

    uint8_t lineTokens[BASIC_LINE_TMP];
    int tokenLength = tokenizeBody(reader, lineTokens, BASIC_LINE_TMP);
    if(tokenLength == -1){
        basicErr = BASIC_ERR_NOMEM;
        return basicErr;
    }
    if(tokenLength == -2){
        basicErr = BASIC_ERR_SYNTAX;
        return basicErr;
    }

    basicErr = storeLine(number, lineTokens, tokenLength);
    return basicErr;
}

void basicReset(void){
    basicCodeTop = 0;
    basicProgCount = 0;
    basicNameCount = 0;
    basicErr = BASIC_OK;
    basicErrLine = 0;
    basicStrReset();
    basicVarsReset();
}

int basicLastError(void){
    return basicErr;
}

int basicErrorLine(void){
    return basicErrLine;
}

const char *basicErrorText(int error){
    switch(error){
        case BASIC_OK:          
            return "OK";
        case BASIC_ERR_SYNTAX:  
            return "SYNTAX ERROR";
        case BASIC_ERR_NOLINE:  
            return "NO SUCH LINE";
        case BASIC_ERR_TYPE:    
            return "TYPE ERROR";
        case BASIC_ERR_DIVZERO: 
            return "DIVIDE BY ZERO";
        case BASIC_ERR_NOMEM:   
            return "OUT OF MEMORY";
        case BASIC_ERR_STACK:   
            return "STACK ERROR";
        case BASIC_ERR_NONEXT:  
            return "NEXT WITHOUT FOR";
        case BASIC_ERR_BREAK:   
            return "BREAK";
        default:                
            return "ERROR";
    }
}