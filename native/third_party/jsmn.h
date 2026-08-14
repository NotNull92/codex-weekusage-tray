#pragma once

#include <stddef.h>

extern "C" {

typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1 << 0,
    JSMN_ARRAY = 1 << 1,
    JSMN_STRING = 1 << 2,
    JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
    JSMN_ERROR_NOMEM = -1,
    JSMN_ERROR_INVAL = -2,
    JSMN_ERROR_PART = -3
};

typedef struct jsmntok {
    jsmntype_t type;
    int start;
    int end;
    int size;
    int parent;
} jsmntok_t;

typedef struct jsmn_parser {
    unsigned int pos;
    unsigned int toknext;
    int toksuper;
} jsmn_parser;

static void jsmn_init(jsmn_parser* parser) {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

static jsmntok_t* jsmn_alloc_token(jsmn_parser* parser, jsmntok_t* tokens, size_t count) {
    if (parser->toknext >= count) {
        return NULL;
    }

    jsmntok_t* token = &tokens[parser->toknext++];
    token->start = -1;
    token->end = -1;
    token->size = 0;
    token->parent = -1;
    return token;
}

static void jsmn_fill_token(jsmntok_t* token, jsmntype_t type, int start, int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser* parser, const char* json, size_t length, jsmntok_t* tokens, size_t count) {
    const int start = static_cast<int>(parser->pos);
    for (; parser->pos < length && json[parser->pos] != '\0'; ++parser->pos) {
        switch (json[parser->pos]) {
            case '\t':
            case '\r':
            case '\n':
            case ' ':
            case ',':
            case ']':
            case '}':
                goto found;
            default:
                break;
        }

        if (json[parser->pos] < 32 || json[parser->pos] >= 127) {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
        }
    }

    parser->pos = start;
    return JSMN_ERROR_PART;

found:
    if (tokens == NULL) {
        --parser->pos;
        return 0;
    }

    jsmntok_t* token = jsmn_alloc_token(parser, tokens, count);
    if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
    }

    jsmn_fill_token(token, JSMN_PRIMITIVE, start, static_cast<int>(parser->pos));
    token->parent = parser->toksuper;
    --parser->pos;
    return 0;
}

static int jsmn_parse_string(jsmn_parser* parser, const char* json, size_t length, jsmntok_t* tokens, size_t count) {
    const int start = static_cast<int>(parser->pos);
    ++parser->pos;

    for (; parser->pos < length && json[parser->pos] != '\0'; ++parser->pos) {
        const char character = json[parser->pos];
        if (character == '"') {
            if (tokens == NULL) {
                return 0;
            }

            jsmntok_t* token = jsmn_alloc_token(parser, tokens, count);
            if (token == NULL) {
                parser->pos = start;
                return JSMN_ERROR_NOMEM;
            }

            jsmn_fill_token(token, JSMN_STRING, start + 1, static_cast<int>(parser->pos));
            token->parent = parser->toksuper;
            return 0;
        }

        if (character != '\\' || parser->pos + 1 >= length) {
            continue;
        }

        ++parser->pos;
        switch (json[parser->pos]) {
            case '"':
            case '/':
            case '\\':
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            case 'u':
                ++parser->pos;
                for (int index = 0; index < 4 && parser->pos < length && json[parser->pos] != '\0'; ++index, ++parser->pos) {
                    const char hex = json[parser->pos];
                    if (!((hex >= '0' && hex <= '9') || (hex >= 'A' && hex <= 'F') || (hex >= 'a' && hex <= 'f'))) {
                        parser->pos = start;
                        return JSMN_ERROR_INVAL;
                    }
                }
                --parser->pos;
                break;
            default:
                parser->pos = start;
                return JSMN_ERROR_INVAL;
        }
    }

    parser->pos = start;
    return JSMN_ERROR_PART;
}

static int jsmn_parse(jsmn_parser* parser, const char* json, size_t length, jsmntok_t* tokens, unsigned int count) {
    int token_count = static_cast<int>(parser->toknext);

    for (; parser->pos < length && json[parser->pos] != '\0'; ++parser->pos) {
        switch (json[parser->pos]) {
            case '{':
            case '[': {
                ++token_count;
                if (tokens == NULL) {
                    break;
                }

                jsmntok_t* token = jsmn_alloc_token(parser, tokens, count);
                if (token == NULL) {
                    return JSMN_ERROR_NOMEM;
                }

                if (parser->toksuper != -1) {
                    jsmntok_t* parent = &tokens[parser->toksuper];
                    if (parent->type == JSMN_OBJECT) {
                        return JSMN_ERROR_INVAL;
                    }
                    ++parent->size;
                }

                token->type = json[parser->pos] == '{' ? JSMN_OBJECT : JSMN_ARRAY;
                token->start = static_cast<int>(parser->pos);
                token->parent = parser->toksuper;
                parser->toksuper = static_cast<int>(parser->toknext) - 1;
                break;
            }
            case '}':
            case ']': {
                if (tokens == NULL) {
                    break;
                }

                const jsmntype_t type = json[parser->pos] == '}' ? JSMN_OBJECT : JSMN_ARRAY;
                if (parser->toknext < 1) {
                    return JSMN_ERROR_INVAL;
                }

                jsmntok_t* token = &tokens[parser->toknext - 1];
                for (;;) {
                    if (token->start != -1 && token->end == -1) {
                        if (token->type != type) {
                            return JSMN_ERROR_INVAL;
                        }
                        token->end = static_cast<int>(parser->pos) + 1;
                        parser->toksuper = token->parent;
                        break;
                    }
                    if (token->parent == -1) {
                        return JSMN_ERROR_INVAL;
                    }
                    token = &tokens[token->parent];
                }
                break;
            }
            case '"': {
                const int result = jsmn_parse_string(parser, json, length, tokens, count);
                if (result < 0) {
                    return result;
                }
                ++token_count;
                if (parser->toksuper != -1 && tokens != NULL) {
                    ++tokens[parser->toksuper].size;
                }
                break;
            }
            case '\t':
            case '\r':
            case '\n':
            case ' ':
                break;
            case ':':
                parser->toksuper = static_cast<int>(parser->toknext) - 1;
                break;
            case ',':
                if (tokens != NULL && parser->toksuper != -1 && tokens[parser->toksuper].type != JSMN_ARRAY && tokens[parser->toksuper].type != JSMN_OBJECT) {
                    parser->toksuper = tokens[parser->toksuper].parent;
                }
                break;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 't':
            case 'f':
            case 'n': {
                if (tokens != NULL && parser->toksuper != -1) {
                    const jsmntok_t* parent = &tokens[parser->toksuper];
                    if (parent->type == JSMN_OBJECT || (parent->type == JSMN_STRING && parent->size != 0)) {
                        return JSMN_ERROR_INVAL;
                    }
                }

                const int result = jsmn_parse_primitive(parser, json, length, tokens, count);
                if (result < 0) {
                    return result;
                }
                ++token_count;
                if (parser->toksuper != -1 && tokens != NULL) {
                    ++tokens[parser->toksuper].size;
                }
                break;
            }
            default:
                return JSMN_ERROR_INVAL;
        }
    }

    if (tokens != NULL) {
        for (int index = static_cast<int>(parser->toknext) - 1; index >= 0; --index) {
            if (tokens[index].start != -1 && tokens[index].end == -1) {
                return JSMN_ERROR_PART;
            }
        }
    }

    return token_count;
}

}
