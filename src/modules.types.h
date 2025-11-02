#pragma once

#include <stdint.h>
#include <stddef.h>

struct tr_module_field_info
{
	uint8_t type; // enum tr_module_field_type
	uintptr_t offset;
	const char* name;
	int16_t x;
	int16_t y;
	float min;
	float max;
	float default_value;
	int16_t min_int;
	int16_t max_int;
};
struct tr_module_info
{
	const char* id;
	size_t struct_size;
	const struct tr_module_field_info* fields;
	uint8_t field_count;
	int width;
	int height;
};
enum tr_module_field_type
{
	TR_MODULE_FIELD_FLOAT,
	TR_MODULE_FIELD_INT,
	TR_MODULE_FIELD_INPUT_FLOAT,
	TR_MODULE_FIELD_INPUT_INT,
	TR_MODULE_FIELD_INPUT_BUFFER,
	TR_MODULE_FIELD_BUFFER,
};

typedef struct tr_module_info tr_module_info_t;
typedef struct tr_module_field_info tr_module_field_info_t;