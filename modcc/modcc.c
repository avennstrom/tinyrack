#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void print_token(stb_lexer *lexer)
{
    switch (lexer->token) {
        case CLEX_eof       : printf("<<<EOF>>>"); break;
        case CLEX_id        : printf("_%s", lexer->string); break;
        case CLEX_eq        : printf("=="); break;
        case CLEX_noteq     : printf("!="); break;
        case CLEX_lesseq    : printf("<="); break;
        case CLEX_greatereq : printf(">="); break;
        case CLEX_andand    : printf("&&"); break;
        case CLEX_oror      : printf("||"); break;
        case CLEX_shl       : printf("<<"); break;
        case CLEX_shr       : printf(">>"); break;
        case CLEX_plusplus  : printf("++"); break;
        case CLEX_minusminus: printf("--"); break;
        case CLEX_arrow     : printf("->"); break;
        case CLEX_andeq     : printf("&="); break;
        case CLEX_oreq      : printf("|="); break;
        case CLEX_xoreq     : printf("^="); break;
        case CLEX_pluseq    : printf("+="); break;
        case CLEX_minuseq   : printf("-="); break;
        case CLEX_muleq     : printf("*="); break;
        case CLEX_diveq     : printf("/="); break;
        case CLEX_modeq     : printf("%%="); break;
        case CLEX_shleq     : printf("<<="); break;
        case CLEX_shreq     : printf(">>="); break;
        case CLEX_eqarrow   : printf("=>"); break;
        case CLEX_dqstring  : printf("\"%s\"", lexer->string); break;
        case CLEX_sqstring  : printf("'\"%s\"'", lexer->string); break;
        case CLEX_charlit   : printf("'%s'", lexer->string); break;
        #if defined(STB__clex_int_as_double) && !defined(STB__CLEX_use_stdlib)
        case CLEX_intlit    : printf("#%g", lexer->real_number); break;
        #else
        case CLEX_intlit    : printf("#%ld", lexer->int_number); break;
        #endif
        case CLEX_floatlit  : printf("%g", lexer->real_number); break;
        default:
            if (lexer->token >= 0 && lexer->token < 256)
                printf("%c", (int) lexer->token);
            else {
                printf("<<<UNKNOWN TOKEN %ld >>>\n", lexer->token);
            }
            break;
    }
}

static int next_token(stb_lexer* lex)
{
    int r = stb_c_lexer_get_token(lex);
#if 0
    print_token(lex);
    printf("\n");
#endif
    return r;
}

typedef struct enum_
{
    const char* id;
    int count;
} enum_t;

enum type_type
{
    TYPE_FLOAT,
    TYPE_INT,
    TYPE_TR_INPUT,
    TYPE_TR_BUF,
    TYPE_ENUM,
};

typedef struct type
{
    enum type_type type;
    const char* enum_name;
} type_t;

typedef struct field
{
    type_t type;
    const char* id;

    int has_attr;
    int attr_x;
    int attr_y;
    float attr_min;
    float attr_max;
    float attr_default;
} field_t;

typedef struct module
{
    const char* id;
    const char* id_upper;

    const char* attr_name;
    int attr_width;
    int attr_height;

    size_t field_count;
    field_t fields[64];
} module_t;

size_t module_count = 0;
module_t modules[64];

size_t enum_count;
enum_t enums[64];

static void parse_type(type_t* type, stb_lexer* lex)
{
    assert(lex->token == CLEX_id);
    if (strcmp(lex->string, "float") == 0)
    {
        type->type = TYPE_FLOAT;
    }
    else if (strcmp(lex->string, "int") == 0)
    {
        type->type = TYPE_INT;
    }
    else if (strcmp(lex->string, "tr_buf") == 0)
    {
        type->type = TYPE_TR_BUF;
    }
    else if (strcmp(lex->string, "tr_input") == 0)
    {
        type->type = TYPE_TR_INPUT;
    }
    else if (strcmp(lex->string, "enum") == 0)
    {
        next_token(lex);
        assert(lex->token == CLEX_id);
        type->type = TYPE_ENUM;
        type->enum_name = strdup(lex->string);
    }
    else
    {
        printf("Unsupported module field type \"%s\"\n", lex->string);
        exit(1);
    }
}

static const char* strdup_upper(const char* str)
{
    char* upper = strdup(str);
    for (char* c = upper; *c != '\0'; ++c) *c = toupper(*c);
    return upper;
}

static void write_module_type_enum(FILE* f, const module_t* modules, size_t count)
{
    fprintf(f, "enum tr_module_type\n");
    fprintf(f, "{\n");
    for (size_t i = 0; i < count; ++i)
    {
        fprintf(f, "\t%s,\n", modules[i].id_upper);
    }
    fprintf(f, "\tTR_MODULE_COUNT\n");
    fprintf(f, "};\n");
}

static void write_module_fields_enum(FILE* f, const module_t* module)
{
    fprintf(f, "enum\n");
    fprintf(f, "{\n");
    for (size_t field_index = 0; field_index < module->field_count; ++field_index)
    {
        const field_t* field = &module->fields[field_index];
        fprintf(f, "\t%s_%s,\n", module->id_upper, field->id);
    }
    fprintf(f, "\t%s_FIELD_COUNT\n", module->id_upper);
    fprintf(f, "};\n");
}

static void write_module_field_infos(FILE* f, const module_t* module)
{
    fprintf(f, "static const struct tr_module_field_info %s__fields[] = {\n", module->id);
    for (size_t i = 0; i < module->field_count; ++i)
    {
        const field_t* field = &module->fields[i];

        int min_int = 0;
        int max_int = 0;
        const char* field_type = NULL;
        
        if (field->type.type == TYPE_ENUM)
        {
            for (int i = 0; i < enum_count; ++i)
            {
                if (strcmp(enums[i].id, field->type.enum_name) == 0)
                {
                    assert(enums[i].count > 0);
                    max_int = enums[i].count - 1;
                }
            }
        }

        if (field->type.type == TYPE_INT || field->type.type == TYPE_ENUM)
        {
            if (field->has_attr)
            {
                field_type = "TR_MODULE_FIELD_INPUT_INT";
            }
            else
            {
                field_type = "TR_MODULE_FIELD_INT";
            }
        }
        else if (field->type.type == TYPE_FLOAT)
        {
            if (field->has_attr)
            {
                field_type = "TR_MODULE_FIELD_INPUT_FLOAT";    
            }
            else
            {
                field_type = "TR_MODULE_FIELD_FLOAT";    
            }
        }
        else if (field->type.type == TYPE_TR_BUF)
        {
            field_type = "TR_MODULE_FIELD_BUFFER";
        }
        else if (field->type.type == TYPE_TR_INPUT)
        {
            field_type = "TR_MODULE_FIELD_INPUT_BUFFER";
        }
        else
        {
            assert(0);
        }
        
        fprintf(f, "\t[%s_%s] = {%s, offsetof(struct %s, %s), \"%s\", %d, %d, %f, %f, %f, %d, %d},\n", 
            module->id_upper, field->id, field_type, module->id, field->id, field->id, 
            field->attr_x, field->attr_y, field->attr_min, field->attr_max, field->attr_default, min_int, max_int);
    }
    fprintf(f, "};\n");
}

static void write_module_typedef(FILE* f, const module_t* module)
{
    fprintf(f, "typedef struct %s %s_t;\n", module->id, module->id);
}

static void write_module_infos(FILE* f, const module_t* modules, size_t count)
{
    fprintf(f, "static const struct tr_module_info tr_module_infos[] = {\n");
    for (size_t i = 0; i < count; ++i)
    {
        const module_t* module = &modules[i];
        const char* name = module->attr_name;
        const int width = module->attr_width;
        const int height = module->attr_height;
        fprintf(f, "\t[%s] = {\"%s\", sizeof(struct %s), %s__fields, %zu, %d, %d},\n", module->id_upper, name, module->id, module->id, module->field_count, width, height);
    }
    fprintf(f, "};");
}

int main(int argc, char** argv)
{
    char *src;
    int src_len;

    {
        FILE *f = fopen("src/modules2.h","rb");
        src = (char *) malloc(1 << 20);
        src_len = f ? (int) fread(src, 1, 1<<20, f) : -1;
        if (src_len < 0) {
            fprintf(stderr, "Error opening file\n");
            free(src);
            fclose(f);
            return 1;
        }
        fclose(f);
    }

    //printf("%s\n", src);

    stb_lexer lex;
    stb_c_lexer_init(&lex, src, src + src_len, malloc(0x10000), 0x10000);

    while (next_token(&lex))
    {
        assert(lex.token == CLEX_id);

        if (strcmp(lex.string, "enum") == 0)
        {
            enum_t* e = &enums[enum_count++];
            memset(e, 0, sizeof(enum_t));

            next_token(&lex);
            assert(lex.token == CLEX_id);
            e->id = strdup(lex.string);
            
            next_token(&lex);
            assert(lex.token = '{');
            
            while (next_token(&lex))
            {
                if (lex.token == '}')
                {
                    break;
                }

                assert(lex.token == CLEX_id);
                next_token(&lex);
                assert(lex.token == ',');

                ++e->count;
            }

            //next_token(&lex);
            assert(lex.token == '}');
            next_token(&lex);
            assert(lex.token == ';');

            continue;
        }

        assert(strcmp(lex.string, "TR_MODULE") == 0);

        module_t* module = &modules[module_count++];
        memset(module, 0, sizeof(module_t));

        next_token(&lex);
        assert(lex.token == '(');

        while (next_token(&lex))
        {
            assert(lex.token == CLEX_id);
            next_token(&lex);
            assert(lex.token == '=');
            
            if (strcmp(lex.string, "Name") == 0)
            {
                next_token(&lex);
                assert(lex.token == CLEX_dqstring);
                module->attr_name = strdup(lex.string);
            }
            else if (strcmp(lex.string, "Width") == 0)
            {
                next_token(&lex);
                assert(lex.token == CLEX_intlit);
                module->attr_width = lex.int_number;
            }
            else if (strcmp(lex.string, "Height") == 0)
            {
                next_token(&lex);
                assert(lex.token == CLEX_intlit);
                module->attr_height = lex.int_number;
            }
            else
            {
                printf("Unsupported module attribute \"%s\"", lex.string);
                exit(1);
            }

            next_token(&lex);
            if (lex.token == ')')
            {
                break;
            }

            assert(lex.token == ',');
        }

        next_token(&lex);
        assert(strcmp(lex.string, "struct") == 0);

        next_token(&lex);
        assert(lex.token == CLEX_id);
        module->id = strdup(lex.string);
        module->id_upper = strdup_upper(lex.string);

        next_token(&lex);
        assert(lex.token == '{');
        
        while (next_token(&lex))
        {
            if (lex.token == '}')
            {
                break;
            }

            field_t* field = &module->fields[module->field_count++];
            memset(field, 0, sizeof(field_t));
            
            if (strcmp(lex.string, "TR_FIELD") == 0)
            {
                field->has_attr = 1;

                next_token(&lex);
                assert(lex.token == '(');

                while (next_token(&lex))
                {
                    assert(lex.token == CLEX_id);
                    next_token(&lex);
                    assert(lex.token == '=');
                    
                    if (strcmp(lex.string, "X") == 0)
                    {
                        next_token(&lex);
                        assert(lex.token == CLEX_intlit);
                        field->attr_x = lex.int_number;
                    }
                    else if (strcmp(lex.string, "Y") == 0)
                    {
                        next_token(&lex);
                        assert(lex.token == CLEX_intlit);
                        field->attr_y = lex.int_number;
                    }
                    else if (strcmp(lex.string, "Min") == 0)
                    {
                        next_token(&lex);
                        assert(lex.token == CLEX_floatlit);
                        field->attr_min = lex.real_number;
                    }
                    else if (strcmp(lex.string, "Max") == 0)
                    {
                        next_token(&lex);
                        assert(lex.token == CLEX_floatlit);
                        field->attr_max = lex.real_number;
                    }
                    else if (strcmp(lex.string, "Default") == 0)
                    {
                        next_token(&lex);
                        assert(lex.token == CLEX_floatlit);
                        field->attr_default = lex.real_number;
                    }
                    else
                    {
                        printf("Unsupported module field attribute \"%s\"\n", lex.string);
                        exit(1);
                    }

                    next_token(&lex);
                    if (lex.token == ')')
                    {
                        break;
                    }

                    assert(lex.token == ',');
                }

                next_token(&lex);
            }

            parse_type(&field->type, &lex);
            
            next_token(&lex);
            assert(lex.token == CLEX_id);
            field->id = strdup(lex.string);

            next_token(&lex);
            assert(lex.token == ';');
        }

        next_token(&lex);
        assert(lex.token == ';');
    }
    
    // for (size_t module_index = 0; module_index < module_count; ++module_index)
    // {
    //     module_t* module = &modules[module_index];
    //     printf("%s\n", module->id);
    //     printf("{\n");
    //     for (size_t field_index = 0; field_index < module->field_count; ++field_index)
    //     {
    //         field_t* field = &module->fields[field_index];
    //         printf("\t %s %s;\n", field->type.type, field->id);
    //     }
    //     printf("}\n");
    // }
    
#define ALL_MODULES_PORTED 1

    FILE* f = fopen("src/modules.generated.h", "wb");
    fprintf(f, "#pragma once\n");
    fprintf(f, "// --------- GENERATED CODE ---------\n");
    fprintf(f, "// --------- DO NOT MODIFY ----------\n");
    fprintf(f, "\n");
    fprintf(f, "#include \"modules.types.h\"\n");
    fprintf(f, "#include \"modules2.h\"\n");
    fprintf(f, "\n");
#if ALL_MODULES_PORTED
    write_module_type_enum(f, modules, module_count);
#endif
    for (size_t module_index = 0; module_index < module_count; ++module_index)
    {
        write_module_fields_enum(f, &modules[module_index]);
        write_module_field_infos(f, &modules[module_index]);
        write_module_typedef(f, &modules[module_index]);
    }
#if ALL_MODULES_PORTED
    write_module_infos(f, modules, module_count);
#endif
    fclose(f);

    return 0;
}