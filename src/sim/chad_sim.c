/*
*
* CHAD simulator
*
*/

#include <chad_sim.h>


// program metadata
instruction instructions[MAX_SIZE_PC];
int instruction_count, sector_count;
FILE *f_dmemout, *f_regout, *f_trace, *f_hwregtrace, *f_cycles, *f_leds, *f_display7seg, *f_diskout, *f_monitor, *f_monitoryuv;
bool irq0, irq1, irq2, irq, irq_routine;


uint32 pc;
uint32 R[COUNT_REGISTERS];
uint32 IORegister[COUNT_IOREGISTERS];
uint32 IORegister_SIZE_IN[COUNT_IOREGISTERS] = {
	0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000FFF, 0x00000FFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000001, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000003, 0x0000007F, 0x00000FFF, 0x00000001, 0x00000000, 0x00000000, 0x0000FFFF, 0x000000FF, 0x00000000
};
uint32 IORegister_SIZE_OUT[COUNT_IOREGISTERS] = {
	0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000FFF, 0x00000FFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000001, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000003, 0x0000007F, 0x00000FFF, 0x00000001, 0x00000000, 0x00000000, 0x0000FFFF, 0x000000FF, 0x00000001
};
uint32 MEM[MAX_DMEM_ITEMS];
uint8 disk[SIZE_HDD_SECTORS_H][SIZE_HDD_SECTORS_W];
uint8 monitor[SIZE_MONITOR_H*SIZE_MONITOR_W];
uint32 irq2in_i, irq2in_count;
uint32 *irq2in_feed;
int irq1_i;

void parse_instruction(char* line, int index) {
	llu binary;
	strip(line);
	hex_to_unsigned_long_long(line, &binary);
	instructions[index].opcode 		= (binary>>40)&0xFF;
	instructions[index].rd			= (binary>>32)&0xF;
	instructions[index].rs			= (binary>>28)&0xF;
	instructions[index].rt			= (binary>>24)&0xF;
	instructions[index].immediate1	= (binary>>12)&0xFFF;
	instructions[index].immediate2	= (binary>>0 )&0xFFF;
}

void read_instructions(char** lines) {
	int i, counted;
	for (i=0, counted=0; lines[i]!=0; i++) {
		if (strlen(lines[i])==0) continue;
		parse_instruction(lines[i], counted);
		counted++;
	}
	instruction_count = counted;
}

void parse_disk_sector(char* line, int index) {
	// TODO
}

void read_disk(char** lines) {
	int i, counted;
	for (i=0, counted=0; lines[i]!=0; i++) {
		if (strlen(lines[i])==0) continue;
		parse_disk_sector(lines[i], counted);
		counted++;
	}
	sector_count = counted;
}

void read_dmemin(char** lines) {
	int i, counted;
	for (i=0, counted=0; lines[i]!=0; i++) {
		if (strlen(lines[i])==0) continue;
		hex_to_uint32(lines[i], &MEM[counted]);
		counted++;
	}
	instruction_count = counted;
}

void parse_irq2in_line(char* line, int index) {
	// TODO: maybe sort
	hex_to_uint32(line, &irq2in_feed[index]);
}

void read_irq2in(char** lines) {
	int i, counted;
	for (i=0, counted=0; lines[i]!=0; i++) {
		if (strlen(lines[i])==0) continue;
		parse_irq2in_line(lines[i], counted);
		counted++;
	}
	irq2in_i = 0;
	irq2in_count = counted;
}

void write_leds() {
	uint32 value = IORegister[leds];
	char* hex = unsigned_long_long_to_hex((llu)value);
	fprintf(f_leds, "%d %s\n", IORegister[clks], hex);
	free(hex);
}

void print_monitor_to_file() {
	int i;
	char* hex;
	for (i=0; i<SIZE_MONITOR; i++) {
		hex = llu_to_hex((llu)monitor[i], 2);
		fprintf(f_monitor, "%s\n", hex);
		free(hex);
	}
}

void write_monitor() {
	uint32 addr, data;
	if (!IORegister[monitorcmd]) return;
	addr = IORegister[monitoraddr];
	data = IORegister[monitordata];
	monitor[addr] = data;
	print_monitor_to_file();
}

// register_write:
// safely writes into a register, crashes if register is invalid
void register_write(int number, uint32 value) {
	if (number < 0 || number >= COUNT_REGISTERS) throw_error(ERROR_RUNTIME, "EXCEPTION IN REGISTER WRITE");
	if (number == 0 || number == 1 || number == 2) return;
	R[number] = value;
}

// register_read:
// safely reads from a register, crashes if register is invalid
uint32 register_read(int number) {
	if (number < 0 || number >= COUNT_REGISTERS) throw_error(ERROR_RUNTIME, "EXCEPTION IN REGISTER READ");
	return R[number];
}

void memory_write(int number, uint32 value) {
	if (number < 0 || number >= MAX_DMEM_ITEMS) throw_error(ERROR_RUNTIME, "EXCEPTION IN MEMORY WRITE");
	MEM[number] = value;
}

uint32 memory_read(int number) {
	if (number < 0 || number >= MAX_DMEM_ITEMS) throw_error(ERROR_RUNTIME, "EXCEPTION IN MEMORY READ");
	return MEM[number];
}

void ioregister_write(int number, uint32 value) {
	if (number < 0 || number >= COUNT_IOREGISTERS) throw_error(ERROR_RUNTIME, "EXCEPTION IN IO REGISTER WRITE");
	IORegister[number] = value & IORegister_SIZE_OUT[number];
	switch (number) {
		case leds:
			write_leds();
			break;
		case monitorcmd:
			write_monitor();
		default:
			break;
	}
}

uint32 ioregister_read(int number) {
	if (number < 0 || number >= COUNT_IOREGISTERS) throw_error(ERROR_RUNTIME, "EXCEPTION IN IO REGISTER READ");
	return IORegister[number] & IORegister_SIZE_IN[number];
}


/*
*
* *** OPCODES ***
* The functions below contain the opcodes
*
*/

void clock_tick() {
	if (IORegister[clks]==0xFFFFFFFF) IORegister[clks]=0;
	else IORegister[clks]++;
}

void add(int rd, int rs, int rt) {
	register_write(rd, register_read(rs)+register_read(rt));
}


void sub(int rd, int rs, int rt) {
	register_write(rd, register_read(rs)-register_read(rt));
}

void and(int rd, int rs, int rt) {
	register_write(rd, register_read(rs)&register_read(rt));
}

void or(int rd, int rs, int rt) {
	register_write(rd, register_read(rs)|register_read(rt));
}

void sll(int rd, int rs, int rt) {
	register_write(rd, register_read(rs)<<register_read(rt));
}

void sra(int rd, int rs, int rt) {
	register_write(rd, (register_read(rs) & 0x80000000) | (register_read(rs) >> register_read(rt)));
}

void srl(int rd, int rs, int rt) {
	register_write(rd, register_read(rs)>>register_read(rt));
}

void beq(int rd, int rs, int rt) {
	if (register_read(rs)==register_read(rt)) pc = register_read(rd) & 0x00000FFF;
}

void bne(int rd, int rs, int rt) {
	if (register_read(rs)!=register_read(rt)) pc = register_read(rd) & 0x00000FFF;
}

void blt(int rd, int rs, int rt) {
	if (register_read(rs)<register_read(rt)) pc = register_read(rd) & 0x00000FFF;
}

void bgt(int rd, int rs, int rt) {
	if (register_read(rs)>register_read(rt)) pc = register_read(rd) & 0x00000FFF;
}

void ble(int rd, int rs, int rt) {
	if (register_read(rs)<=register_read(rt)) pc = register_read(rd) & 0x00000FFF;
}

void bge(int rd, int rs, int rt) {
	if (register_read(rs)>=register_read(rt)) pc = register_read(rd) & 0x00000FFF;
}

void jal(int rd, int rs, int rt) {
	register_write(ra, pc+1);
	pc = register_read(rd) & 0x00000FFF;
}

void lw(int rd, int rs, int rt) {
	register_write( rd, memory_read( ((int)register_read(rs))+((int)register_read(rt)) ) );
}

void sw(int rd, int rs, int rt) {
	memory_write( ((int)register_read(rs))+((int)register_read(rt)), register_read(rd) );
}

void reti(int rd, int rs, int rt) {
	pc = ioregister_read(irqreturn);
	irq_routine = 0;
}

void in(int rd, int rs, int rt) {
	register_write(rd, ioregister_read(((int)register_read(rs))+((int)register_read(rt))));
}

void out(int rd, int rs, int rt) {
	ioregister_write(((int)register_read(rs))+((int)register_read(rt)),register_read(rd));
}

void halt(int rd, int rs, int rt) {
	pc = instruction_count;
}

void (*opcode_fn[COUNT_OPCODES])(int, int, int) = {
	add, sub, and, or, sll, sra, beq, bne, blt, bgt, ble, bge, jal, lw, sw, reti, in, out, halt
};

/*
*
* *** OPCODES END ***
*
*/


/* Perform current instruction:
* Performs the instruction at pc
*/
void perform_current_instruction() {
	instruction ins = instructions[pc];
	if (ins.opcode < 0 || ins.opcode >= COUNT_OPCODES) throw_error(ERROR_RUNTIME, "INVALID OPCODE");
	if (ins.rd < 0 || ins.rd > COUNT_REGISTERS
		|| ins.rs < 0 || ins.rs > COUNT_REGISTERS
		|| ins.rt < 0 || ins.rt > COUNT_REGISTERS) throw_error(ERROR_RUNTIME, "INVALID REGISTER");

	R[imm1] = ins.immediate1;
	R[imm2] = ins.immediate2;
	(*opcode_fn[ins.opcode])(ins.rd, ins.rs, ins.rt);
	pc++;
}

void check_interrupt_0() {
	// TIMER
	irq0 = IORegister[irq0enable] & IORegister[irq0status];
	irq |= irq0;
	if (IORegister[timerenable] == 0) return;
	if (IORegister[timercurrent] < IORegister[timermax]) {
		IORegister[timercurrent] += 1;
	} else {
		IORegister[timercurrent] = 0;
		IORegister[irq0status] = 1;
	}
}

void check_interrupt_1() {
	// DISK
	irq1 = IORegister[irq1enable] & IORegister[irq1status];
	irq |= irq1;
	
	// TODO after 1024 runs and copying via DMA
	IORegister[diskcmd] = 0;
	IORegister[diskstatus] = 0;
}

void check_interrupt_2() {
	// EXTERNAL
	if (irq2in_i >= irq2in_count || irq2in_i < 0) return;
	if (irq2in_feed[irq2in_i]==IORegister[clks]) {
		IORegister[irq2status] = 1;
		irq2in_i++;
	}
	irq2 = IORegister[irq2enable] & IORegister[irq2status];
	irq |= irq2;
}

void check_interrupts() {
	irq = 0;
	check_interrupt_0();
	check_interrupt_1();
	check_interrupt_2();
	if (irq == 1) {
		if (irq_routine) {
			return;
		} else {
			irq_routine = 1;
			IORegister[irqreturn] = pc;
			pc = IORegister[irqhandler];
		}
		
	} else {
		return;
	}
}

void perform_instruction_loop() {
	int i;
	irq_routine = 0;
	
	for (i=0; i<COUNT_REGISTERS; i++) R[i] = 0;
	for (i=0; i<COUNT_IOREGISTERS; i++) IORegister[i] = 0;
	for (i=0; i<MAX_DMEM_ITEMS; i++) MEM[i] = 0;
	IORegister[irq0enable] = 1;
	IORegister[irq1enable] = 1;
	IORegister[irq2enable] = 1;
	IORegister[timerenable] = 1;
	
	pc = 0;
	while (0 <= pc && pc < instruction_count) {
		check_interrupts();
		perform_current_instruction();
		clock_tick();
	}
}


int main(int argc, char *argv[]) {
	// %s <imemin.txt> <dmemin.txt> <diskin.txt> <irq2in.txt>
	int i;
	char **instructions_text, **dmemin_text, **diskin_text, **irq2in_text;
	
	if (argc != 15) throw_error(ERROR_PARAMETERS_SIM, argv[0]); // god help me
	
	get_file_lines(argv[1], &instructions_text);
	read_instructions(instructions_text);
	free_lines(instructions_text);
	
	get_file_lines(argv[2], &dmemin_text);
	read_dmemin(dmemin_text);
	free_lines(dmemin_text);
	
	get_file_lines(argv[3], &diskin_text);
	read_disk(diskin_text);
	free_lines(diskin_text);
	
	get_file_lines(argv[4], &irq2in_text);
	read_disk(diskin_text);
	free_lines(diskin_text);
	
	if ((f_dmemout = fopen(argv[5], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[5]);
	if ((f_regout = fopen(argv[6], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[6]);
	if ((f_trace = fopen(argv[7], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[7]);
	if ((f_hwregtrace = fopen(argv[8], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[8]);
	if ((f_cycles = fopen(argv[9], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[9]);
	if ((f_leds = fopen(argv[10], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[10]);
	if ((f_display7seg = fopen(argv[11], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[11]);
	if ((f_diskout = fopen(argv[12], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[12]);
	if ((f_monitor = fopen(argv[13], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[13]);
	if ((f_monitoryuv = fopen(argv[14], "r"))==NULL) throw_error(ERROR_FILE_ACCESS, argv[14]);
	
	perform_instruction_loop();
	
	fclose(f_dmemout);
	fclose(f_regout);
	fclose(f_trace);
	fclose(f_hwregtrace);
	fclose(f_cycles);
	fclose(f_leds);
	fclose(f_display7seg);
	fclose(f_diskout);
	fclose(f_monitor);
	fclose(f_monitoryuv);
	return 0;
}
