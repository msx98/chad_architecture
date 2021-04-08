#ifndef H_CHAD_UTILS
#define H_CHAD_UTILS

#define DEBUG 1



#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

typedef bool			uint1;
typedef unsigned char		uint2;
typedef unsigned char		uint4;
typedef u_int8_t		uint7;
typedef u_int8_t		uint8;
typedef u_int16_t		uint12;
typedef u_int16_t		uint16;
typedef u_int16_t		uint16;
typedef u_int32_t		uint32;
typedef u_int64_t		uint64;
typedef unsigned long long	llu;

#define MAX_SIZE_PC		4096
#define MAX_SIZE_LINE		500
#define MAX_SIZE_LABEL		50
#define MAX_DMEM_ITEMS		4096
#define MAX_DMEM_CELL		8
#define FLAG_REGULAR		0
#define FLAG_LABEL		1
#define FLAG_DOTWORD		2
#define ERROR_FILE_ACCESS	-1000
#define ERROR_COMPILE_TIME	-2000
#define ERROR_PARAMETERS	-3000
#define ERROR_PARAMETERS_SIM	-3001
#define ERROR_UNKNOWN_OPCODE	-4000
#define ERROR_UNKNOWN_REGISTER -5000
#define COMPILED_SUCCESSFULLY	-10000
#define ERROR_RUNTIME		-6000
#define CHAR_COMMENT		'#'
#define COUNT_REGISTERS	16
#define COUNT_IOREGISTERS	23
#define COUNT_OPCODES		20

#define debug_print(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while(0)

typedef struct t_instruction {
#if DEBUG==1
	char* line;
#endif
	unsigned char opcode;		// 8 bits	47:40
	unsigned char rd;		// 4 bits	39:36
	unsigned char rs;		// 4 bits	35:32
	unsigned char rt;		// 4 bits	31:28
	unsigned char reserved;	// 4 bits	27:24
	unsigned int immediate1;	// 12 bits	23:12
	unsigned int immediate2;	// 12 bits	11:0
} instruction;

typedef struct t_label {
	unsigned char* name;
	unsigned int pointer; // to pc
} label;

typedef struct t_dotword {
	unsigned int address;
	unsigned int value;
} dotword;

extern char CHARSET_HEX[16];
extern char* STR_OPCODES[COUNT_OPCODES];
extern char* STR_REGISTERS[COUNT_REGISTERS];
extern char* STR_IOREGISTERS[COUNT_IOREGISTERS];

unsigned long HASH_OPCODES[COUNT_OPCODES];
unsigned long HASH_REGISTERS[COUNT_REGISTERS];
unsigned long HASH_IOREGISTERS[COUNT_IOREGISTERS];

void throw_error(const int reason, const char* details);
void strip(char* in);
bool is_whitespace(char c);
bool hex_to_unsigned_int(char* in, unsigned int* out);
bool hex_to_unsigned_long_long(char* in, unsigned long long* out);
bool hex_to_uint32(char* in, uint32* out);
char* unsigned_long_long_to_hex(unsigned long long number);
char* llu_to_hex(llu number, int min_width);
char* unsigned_int_to_hex(unsigned int number);
bool char_to_unsigned_int(char* in, unsigned int* out);
int count_occ(char* line, char c);
unsigned long hash(unsigned char *str);
int split(char* s, char del, char*** result);
void free_lines(char** lines);
void pop_char(char* line, int index);
unsigned int* memtext_to_uint_arr(char** lines);
long get_file_size(FILE *f);
char* get_file_str(char* path);
int get_file_lines(char* path, char*** lines);


#endif
