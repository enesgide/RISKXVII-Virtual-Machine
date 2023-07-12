#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

struct heap_memory {
    uint8_t first_bank;
	uint8_t banks_used;	
	uint8_t next;
	int value;
	
};
typedef struct heap_memory HeapMemory;

struct virtual_machine {
    unsigned int program_counter;
    unsigned int registers[32];

    //Memory types
    int instruction_memory[256]; // 0x000-0x3ff
    int data_memory[256]; // 0x400-0x7ff
    //virtual routines 0x800 - 0x8ff
	HeapMemory heap_banks[128];
};
typedef struct virtual_machine VirtualMachine;

struct instruction {
    char binary[33];
    uint8_t opcode;
    char type[3];
};
typedef struct instruction Instruction;

void get_instruction_type(Instruction* instruction) {
	uint8_t opcode = instruction->opcode;
	
	switch (opcode) {
		case 0b0110011:
			strcpy(instruction->type, "R");
			break;
		case 0b0010011:
		case 0b0000011:
		case 0b1100111:
			strcpy(instruction->type, "I");
			break;
		case 0b0100011:
			strcpy(instruction->type, "S");
			break;
		case 0b1100011:
			strcpy(instruction->type, "SB");
			break;
		case 0b0110111:
			strcpy(instruction->type, "U");
			break;
		case 0b1101111:
			strcpy(instruction->type, "UJ");
			break;
		default:
			strcpy(instruction->type, "XX");
			break;
	}
}

// Error handling
void register_dump(VirtualMachine* vm) {
    printf("PC = 0x%08x;\n", vm->program_counter);
    for (int i = 0; i < 32; i++) {
        printf("R[%d] = 0x%08x;\n", i, (unsigned int) vm->registers[i]);
    }
}

void fake_instruction(VirtualMachine* vm, int num) {
    printf("Instruction Not Implemented: 0x%08x\n", (unsigned int) num);
    register_dump(vm);
}

void illegal_operation(VirtualMachine* vm) {
	unsigned int current_instruction = (unsigned int) (vm->program_counter / 4);
    printf("Illegal Operation: 0x%08x\n", vm->instruction_memory[current_instruction]);
    register_dump(vm);
	exit(1);
}

// Heap banks
int get_next_allocation(VirtualMachine* vm, uint8_t start_index) {
	for (int i = start_index; i < 128; i++) {
		HeapMemory* memory_item = &(vm->heap_banks[i]);
		if (memory_item->first_bank != 255) {
			return i;
		}
	}
	return -1;
}

void fix_previous_allocations(VirtualMachine* vm, uint8_t start_index, uint8_t check_next, uint8_t new_next) {
	for (int i = start_index; i >= 0; i--) {
		HeapMemory* memory_item = &(vm->heap_banks[i]);
		if (memory_item->first_bank != 255) {
			if (memory_item->next == check_next) {
				memory_item->next = new_next;
			}
		}
	}
}

void my_malloc(VirtualMachine* vm, int size) {
    uint8_t success = 0;
    uint8_t banks_required = (uint8_t) (size / 64);
	uint8_t remainder = size % 64;
	
	if (remainder > 0) {
		banks_required++;
	}
	
    if (banks_required == 0 || banks_required > 128) {
        vm->registers[28] = 0;
        return;
    }	

    uint8_t free_banks = 0;
    short first_bank = -1;

    for (int i = 0; i < 128; i++) {
		HeapMemory* memory_item = &(vm->heap_banks[i]);
        if (memory_item->first_bank == 255) {
            if (first_bank == -1) {
                first_bank = i;
            }
            free_banks++;
            if (free_banks == banks_required) {
                success = 1;
				
				for (int i = first_bank; i < (first_bank + banks_required); i++) {
					HeapMemory* iterated_memory_item = &(vm->heap_banks[i]);
					iterated_memory_item->first_bank = first_bank;
					iterated_memory_item->banks_used = banks_required;
					iterated_memory_item->value = 0;
					iterated_memory_item->next = get_next_allocation(vm, first_bank + banks_required);
				}
				fix_previous_allocations(vm, first_bank-1, memory_item->next, first_bank);
				
                break;
            }
        } else {
            first_bank = -1;
            free_banks = 0;
        }
    }

    if (success == 1) {
        vm->registers[28] = first_bank * 64 + 0xb700;
    } else {
        vm->registers[28] = 0;
    }
}

void error_check_heap_bank(VirtualMachine* vm, int address) {		
	if (address < 0xb700 || address > 0xd700) {
        illegal_operation(vm);
	}
	
    if (address % 64 != 0) { // not start of bank
        illegal_operation(vm);
    }
    
	int bank_index = (address - 0xb700) / 64;	
	HeapMemory* memory_item = &(vm->heap_banks[bank_index]);
	
	if (memory_item->first_bank == 255) {
		illegal_operation(vm); // not allocated
	}
}

void my_free(VirtualMachine* vm, int address) {
	error_check_heap_bank(vm, address);
	
	int bank_index = (address - 0xb700) / 64;
	HeapMemory* memory_item = &(vm->heap_banks[bank_index]);
	
	for (int i = bank_index; i >= 0; i--) {
		HeapMemory* iterated_memory_item = &(vm->heap_banks[i]);
		if (iterated_memory_item->first_bank != 255) {
			HeapMemory* prev_alloc = &(vm->heap_banks[i]);
			if (prev_alloc->next == iterated_memory_item->first_bank) {
				prev_alloc->next = memory_item->next;
			}
			break;
		}
	}
	
	for (int i = bank_index; i < 128; i++) {
		HeapMemory* iterated_memory_item = &(vm->heap_banks[i]);
		if (iterated_memory_item->first_bank == memory_item->first_bank) {
			iterated_memory_item->first_bank = 255;
		}
	}
}

int get_bank_value(VirtualMachine* vm, int address) {
	error_check_heap_bank(vm, address);
	
	int bank_index = (address - 0xb700) / 64;
	HeapMemory* memory_item = &(vm->heap_banks[bank_index]);
	
	return memory_item->value;
}

void set_bank_value(VirtualMachine* vm, int address, long val) {
	error_check_heap_bank(vm, address);
	
	int bank_index = (address - 0xb700) / 64;
	HeapMemory* memory_item = &(vm->heap_banks[bank_index]);
		
	memory_item->value = val;
}

// Virtual routines
int check_virtual_routine(VirtualMachine* vm, int vr_id, uint8_t register_index) {
	if (vr_id == 0) {
        printf("%c", vm->registers[register_index]);
    } else if (vr_id == 1) {
        printf("%d", vm->registers[register_index]);
    } else if (vr_id == 2) {
        printf("%x", (unsigned int) vm->registers[register_index]);
    } else if (vr_id == 3) {
        printf("CPU Halt Requested\n");
        exit(0);
    } else if (vr_id == 4) {
        char c;
        int n = scanf("%c\n", &c);
		if (n > 0) {
			vm->registers[register_index] = (unsigned int) c;
		}        
    } else if (vr_id == 5) {
        int num;
        int n = scanf("%d", &num);
		if (n > 0) {
			vm->registers[register_index] = (unsigned int) num;
		}        
    } else if (vr_id == 6) {
        printf("%x", vm->program_counter);
    } else if (vr_id == 7) {
        register_dump(vm);
    } else if (vr_id == 8) {
        printf("%x", (unsigned int) vm->registers[register_index]);
    }
	return 0;
}

// Convert from binary to decimal
int binary_to_decimal(char* binary) {
    int decimal = 0;
    for (int i = 0; i < strlen(binary); i++) {
        if (binary[i] == '1') {
            decimal += pow(2, (strlen(binary)-1)-i);
        }
    }
    return decimal;
}

// Convert instruction from uint to binary string
void decimal_to_binary(char* binary, unsigned int num) {
    binary[32] = '\0';
    for (int i = 31; i >= 0; i--) {
        if (num >= pow(2,i)) {
            num -= pow(2,i);
            binary[31-i] = '1';
        } else {
            binary[31-i] = '0';
        }
    }
}

// Store binary substring into dest (component binary)
void assign_format_binary(char* binary, int offset, char* dest, int dest_size) {
    for (int i = 0; i < dest_size - 1; i++) {
        dest[i] = binary[i + offset];
    }
    dest[dest_size - 1] = '\0';
}

char convert_twos_complement(char* binary) {
    if (binary[0] == '0') {
        return '+';
    }
	
    int count = 0;
    for (int i = 0; binary[i] != '\0'; i++) {
        count++;
        if (binary[i] == '0') {
            binary[i] = '1';
        }  else if (binary[i] == '1') {
            binary[i] = '0';
        }
    }
	
    for (int i = count - 1; i >= 0; i--) {
        if (binary[i] == '0') {
            binary[i] = '1';
            break;
        } else if (binary[i] == '1') {
            binary[i] = '0';
        }
    }
    return '-';
}

// Bit manipulation
int sext(int last_bits) {
    if ((last_bits & 0x80) == 0x80) {
        return last_bits | 0xFFFFFF00;
    } else {
        return last_bits | 0x00;
    }
}

int bit8_shift(int remainder, int num) {
    if (remainder == 0) {
        num = num & 0xFF;
    } else if (remainder == 1) {
        num = num & 0xFF00;
        num = num >> 8;
    } else if (remainder == 2) {
        num = num & 0xFF0000;
        num = num >> 16;
    }  else {
        num = num & 0xFF000000;
        num = num >> 24;
    }
    return num;
}

int bit16_shift(int remainder, int num, int nextNum) {
    if (remainder == 0) {
        num = (num & 0xFF << 8) | ((nextNum & 0xFF000000) >> 24);
    } else if (remainder == 1) {
        num = num & 0xFFFF;
        num = num >> 8;
    } else if (remainder == 2) {
        num = num & 0xFFFF00;
        num = num >> 8;
    }  else {
        num = num & 0xFFFF0000;
        num = num >> 16;
    }
    return num;
}

int bit32_shift(int remainder, int num, int nextNum) {
    if (remainder == 0) {
        num = ((num & 0xFF) << 24) | ((nextNum & 0xFFFFFF00) >> 8);
    } else if (remainder == 1) {
        num = ((num & 0xFFFF) << 16) | ((nextNum & 0xFFFF0000) >> 16);
    } else if (remainder == 2) {
        num = ((num & 0xFFFFFF) << 8) | ((nextNum & 0xFF000000) >> 24);
    }  else {
        num = num & 0xFFFFFFFF;
    }
    return num;
}

int bit8_store(int remainder, int num, int new_value) {
    if (remainder == 0) {
        num = (num & 0xFFFFFF00) | (new_value & 0xFF);
    } else if (remainder == 1) {
        num = (num & 0xFFFF00FF) | (new_value & 0xFF00);
    } else if (remainder == 2) {
        num = (num & 0xFF00FFFF) | (new_value & 0xFF0000);
    }  else {
        num = (num & 0x00FFFFFF) | (new_value & 0xFF000000);
    }
    return num;
}

void bit16_store(int remainder, int num1, int num2, int new_value, int* value1, int* value2) {
    if (remainder == 0) {
        *value1 = (num1 & 0xFFFFFF00) | ((new_value & 0xFF00) >> 8);
		*value2 = (num2 & 0x00FFFFFF) | ((new_value & 0x00FF) << 24);
    } else if (remainder == 1) {
        *value1 = (num1 & 0xFFFF0000) | (new_value & 0xFFFF);
    } else if (remainder == 2) {
        *value1 = (num1 & 0xFF0000FF) | ((new_value & 0xFFFF) << 8);
    }  else {
        *value1 = (num1 & 0xFFFF) | ((new_value & 0xFFFF) << 16);
    }
}

void bit32_store(int remainder, int num1, int num2, int new_value, int* value1, int* value2) {
    if (remainder == 0) {
        *value1 = (num1 & 0xFFFFFF00) | ((new_value & 0xFF000000) >> 24);
		*value2 = (num2 & 0x000000FF) | ((new_value & 0x00FFFFFF) << 8);
    } else if (remainder == 1) {
        *value1 = (num1 & 0xFFFF0000) | ((new_value & 0xFFFF0000) >> 16);
		*value2 = (num2 & 0x0000FFFF) | ((new_value & 0x0000FFFF) << 16);
    } else if (remainder == 2) {
        *value1 = (num1 & 0xFF000000) | ((new_value & 0x00FFFFFF) >> 8);
		*value2 = (num2 & 0x00FFFFFF) | ((new_value & 0x000000FF) << 24);
    }  else {
        *value1 = num1 | new_value;
    }
}

int get_next_num(VirtualMachine* vm, int address) {
    if (address >= 0 && address <= 1023) {
        if (address >= 1021) {
            return vm->data_memory[0];
        }
        return vm->instruction_memory[address / 4 + 1];
    } else if (address >= 1024 && address <= 2047) {
        if (address >= 2045) {
            return 0;
        }
        return vm->data_memory[(address - 1024) / 4 + 1];
    }
	return 0;
}

// Arithmetic and logic operations
void add(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] + vm->registers[rs2];
    }    
}

void addi(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] + imm;
    }
}

void sub(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] - vm->registers[rs2];
    }
}

void lui(VirtualMachine* vm, uint8_t rd, int imm) {
    if (rd != 0) {
        vm->registers[rd] = imm * pow(2,12);
    }
}

void xor(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] ^ vm->registers[rs2];
    }
}

void xori(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] ^ imm;
    }
}

void or(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] | vm->registers[rs2];
    }
}

void ori(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] | imm;
    }
}

void and(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] & vm->registers[rs2];
    }
}

void andi(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    if (rd != 0) {
        vm->registers[rd] = vm->registers[rs1] & imm;
    }
}

void sll(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
	//printf("sll (%d) ", vm->program_counter); 
    if (rd != 0) {
        vm->registers[rd] =  vm->registers[rs1] << vm->registers[rs2];
    }
}

void srl(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] =  vm->registers[rs1] >> vm->registers[rs2];
    }
}

void sra(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    int value = vm->registers[rs1];
    for (int i = 0; i < vm->registers[rs2]; i++) {
        if ((value & 0b01) > 0) {
            value = value >> 1;
            value = value | 0b10000000;
        } else {
            value = value >> 1;
        }
    }
    if (rd != 0) {
        vm->registers[rd] = value;
    }
}

// Memory operations
void lb(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    int address = vm->registers[rs1] + imm;
    int remainder = address % 4;

    if (address >= 0 && address <= 1023) {
        address = address / 4;
        int last_bits = bit8_shift(remainder, vm->instruction_memory[address] & 0xFF);
        if (rd != 0) {
            vm->registers[rd] = sext(last_bits);
        }
    } else if (address >= 1024 && address <= 2047) {
        address = (address - 1024) / 4;
        int last_bits = bit8_shift(remainder, vm->data_memory[address] & 0xFF);
        if (rd != 0) {
            vm->registers[rd] = sext(last_bits);
        }
    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rd);
    } else {
		vm->registers[rd] = get_bank_value(vm, address) & 0xFF;
	}
}

void lh(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    int address = vm->registers[rs1] + imm;
	int next_num = get_next_num(vm, address);
	int remainder = address % 4;

    if (address >= 0 && address <= 1023) {
        address = address / 4;
        int last_bits = bit16_shift(remainder, vm->instruction_memory[address] & 0xFFFF, next_num);
        if (rd != 0) {
            vm->registers[rd] = sext(last_bits);
        }
    } else if (address >= 1024 && address <= 2047) {
        address = (address - 1024) / 4;
        int last_bits = bit16_shift(remainder, vm->data_memory[address] & 0xFFFF, next_num);
        if (rd != 0) {
            vm->registers[rd] = sext(last_bits);
        }
    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rd);
    } else {
		vm->registers[rd] = get_bank_value(vm, address) & 0xFFFF;
	}
}

void lw(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    int address = vm->registers[rs1] + imm;
    int next_num = get_next_num(vm, address);
    int remainder = address % 4;

    if (address >= 0 && address <= 1023) {
        address = address / 4;
        int last_bits = bit32_shift(remainder, vm->instruction_memory[address], next_num);
        if (rd != 0) {
            vm->registers[rd] = last_bits;
        }
    } else if (address >= 1024 && address <= 2047) {
        address = (address - 1024) / 4;
        int last_bits = bit32_shift(remainder, vm->data_memory[address], next_num);
        if (rd != 0) {
            vm->registers[rd] = last_bits;
        }
    } else if (address >= 2048 && address <= 2303) {
		if (address >= 2070) {
            address -= 2;
            remainder = address % 4;
        }
		int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rd);
    } else {
		vm->registers[rd] = get_bank_value(vm, address);
	}
}

void lbu(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    int address = vm->registers[rs1] + imm;
    int remainder = address % 4;

    if (address >= 0 && address <= 1023) {
        address = address / 4;
        unsigned int last_bits = bit8_shift(remainder, (unsigned int) vm->instruction_memory[address]);
        if (rd != 0) {
            vm->registers[rd] = last_bits;
        }
    } else if (address >= 1024 && address <= 2047) {
        address = (address - 1024) / 4;
        unsigned int last_bits = bit8_shift(remainder, (unsigned int) vm->data_memory[address]);
        if (rd != 0) {
            vm->registers[rd] = last_bits;
        }
    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rd);
    } else {
		vm->registers[rd] = (unsigned int) (get_bank_value(vm, address) & 0xFF);
	}
}

void lhu(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    int address = vm->registers[rs1] + imm;
    int nextNum = get_next_num(vm, address);
    int remainder = address % 4;

    if (address >= 0 && address <= 1023) {
        address = address / 4;
        unsigned int last_bits = bit16_shift(remainder, (unsigned int) vm->instruction_memory[address], nextNum);
        if (rd != 0) {
            vm->registers[rd] = last_bits;
        }
    } else if (address >= 1024 && address <= 2047) {
        address = (address - 1024) / 4;
        unsigned int last_bits = bit16_shift(remainder, (unsigned int) vm->data_memory[address], nextNum);
        if (rd != 0) {
            vm->registers[rd] = last_bits;
        }
    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rd);
    } else {
		vm->registers[rd] = (unsigned int) (get_bank_value(vm, address) & 0xFFFF);
	}
}

void sb(VirtualMachine* vm, uint8_t rs1, int imm, uint8_t rs2) {
    int address = vm->registers[rs1] + imm;
	int remainder = address % 4;

    if (address >= 1024 && address <= 2047) {
        address = (address - 1024) / 4;
        int new_value = bit8_store(remainder, vm->data_memory[address], vm->registers[rs2]);
        vm->data_memory[address] = new_value;
		
	} else if (address == 2096) {
		my_malloc(vm, vm->registers[rs2] && 0xFF);
    } else if (address == 2100) {
		my_free(vm, vm->registers[rs2] && 0xFF);

    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rs2);
    } else {
		set_bank_value(vm, address, vm->registers[rs2] && 0xFF);
	}
}

void sh(VirtualMachine* vm, uint8_t rs1, int imm, uint8_t rs2) {
    int address = vm->registers[rs1] + imm;
	int remainder = address % 4;	

    if (address >= 1024 && address <= 2046) {
		int use_var_2s = 0;
		int value1 = 0;
		int value2 = 0;
		int num1 = vm->registers[rs2];
		int num2 = 0;
		int* next_address;
		
		if (address < 2047 && remainder <= 0) {
			use_var_2s = 1;
			next_address = &(vm->data_memory[((int) address / 4) - 1]);
		}

        address = (address - 1024) / 4;
		bit16_store(remainder, num1, num2, vm->registers[rs2], &value1, &value2);
        vm->data_memory[address] = value1;
		if  (use_var_2s == 1) {
			*next_address = value2;
		}
	
	} else if (address == 2096) {
		my_malloc(vm, vm->registers[rs2] && 0xFFFF);
    } else if (address == 2100) {
		my_free(vm, vm->registers[rs2] && 0xFFFF);
		
    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rs2);
    } else {
		set_bank_value(vm, address, vm->registers[rs2] && 0xFFFF);
	}
}

void sw(VirtualMachine* vm, uint8_t rs1, int imm, uint8_t rs2) {
    int address = vm->registers[rs1] + imm;
	int remainder = address % 4;

    if (address >= 1024 && address <= 2044) {
		int use_var_2s = 0;
		int value1 = 0;
		int value2 = 0;
		int num1 = vm->registers[rs2] & 0xFFFFFFFF;
		int num2 = 0;
		int* next_address;
		
		if (address >= 2047) {
			
		} else if (remainder <= 0) {
			use_var_2s = 1;
			next_address = &(vm->data_memory[((int) address / 4) - 1]);
		}

        address = (address - 1024) / 4;
		bit32_store(remainder, num1, num2, vm->registers[rs2], &value1, &value2);
        vm->data_memory[address] = value1;
		if  (use_var_2s == 1) {
			*next_address = value2;
		}
		
	} else if (address == 2096) {
		my_malloc(vm, vm->registers[rs2]);
    } else if (address == 2100) {
		my_free(vm, vm->registers[rs2]);
		
    } else if (address >= 2048 && address <= 2303) {
        int index = (address - 2048) / 4;
        check_virtual_routine(vm, index, rs2);
    } else {
		set_bank_value(vm, address, vm->registers[rs2]);
	}
}

// Program flow operations
void slt(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (rd != 0) {
        vm->registers[rd] = (vm->registers[rs1] < vm->registers[rs2]) ? 1 : 0;
    }
}

void slti(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    if (rd != 0) {
        vm->registers[rd] = (vm->registers[rs1] < imm) ? 1 : 0;
    }
}

void sltu(VirtualMachine* vm, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    unsigned int rs1_value = (unsigned int) vm->registers[rs1];
    unsigned int rs2_value = (unsigned int) vm->registers[rs2];
    if (rd != 0) {
        vm->registers[rd] = (rs1_value < rs2_value) ? 1 : 0;
    }
}

void sltiu(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    unsigned int rs1_value = (unsigned int) vm->registers[rs1];
    unsigned int imm_value = (unsigned int) imm;
    if (rd != 0) {
        vm->registers[rd] = (rs1_value < imm_value) ? 1 : 0;
    }
}

void beq(VirtualMachine* vm, uint8_t rs1, uint8_t rs2, int imm) {
    if (vm->registers[rs1] == vm->registers[rs2]) {
        vm->program_counter = vm->program_counter + (imm << 1);
        return;
    }
    vm->program_counter += 4;
}

void bne(VirtualMachine* vm, uint8_t rs1, uint8_t rs2, int imm) {
    if (vm->registers[rs1] != vm->registers[rs2]) {
        vm->program_counter = vm->program_counter + (imm << 1);
        return;
    }
    vm->program_counter += 4;
}

void blt(VirtualMachine* vm, uint8_t rs1, uint8_t rs2, int imm) {
    if (vm->registers[rs1] < vm->registers[rs2]) {
        vm->program_counter = vm->program_counter + (imm << 1);		
        return;
    }
    vm->program_counter += 4;
}

void bltu(VirtualMachine* vm, uint8_t rs1, uint8_t rs2, int imm) {
    unsigned int rs1_value = (unsigned int) vm->registers[rs1];
    unsigned int rs2_value = (unsigned int) vm->registers[rs2];
    unsigned int imm_value = (unsigned int) imm;
    if (rs1_value < rs2_value) {
        vm->program_counter = vm->program_counter + (imm_value << 1);
        return;
    }
    vm->program_counter += 4;
}

void bge(VirtualMachine* vm, uint8_t rs1, uint8_t rs2, int imm) {
    if (vm->registers[rs1] >= vm->registers[rs2]) {
        vm->program_counter = vm->program_counter + (imm << 1);
        return;
    }
    vm->program_counter += 4;
}

void bgeu(VirtualMachine* vm, uint8_t rs1, uint8_t rs2, int imm) {
    unsigned int rs1_value = (unsigned int) vm->registers[rs1];
    unsigned int rs2_value = (unsigned int) vm->registers[rs2];
    unsigned int imm_value = (unsigned int) imm;
    if (rs1_value >= rs2_value) {
        vm->program_counter = vm->program_counter + (imm_value << 1);
        return;
    }
    vm->program_counter += 4;
}

void jal(VirtualMachine* vm, uint8_t rd, int imm) {
    if (rd != 0) {
        vm->registers[rd] = vm->program_counter + 4;
    }
    vm->program_counter = vm->program_counter + (imm << 1);
}

void jalr(VirtualMachine* vm, uint8_t rd, uint8_t rs1, int imm) {
    if (rd != 0) {
        vm->registers[rd] = vm->program_counter + 4;
    }
    vm->program_counter = vm->registers[rs1] + imm;
}

// Format types and execute instructions
void execute_R(VirtualMachine* vm, Instruction* instruction) {
    uint8_t func7;
    char func7_binary[8];
    assign_format_binary(instruction->binary, 0, func7_binary, 8);
    func7 = (uint8_t) binary_to_decimal(func7_binary);

    uint8_t rs2;
    char rs2_binary[6];
    assign_format_binary(instruction->binary, 7, rs2_binary, 6);
    rs2 = (uint8_t) binary_to_decimal(rs2_binary);

    uint8_t rs1;
    char rs1_binary[6];
    assign_format_binary(instruction->binary, 12, rs1_binary, 6);
    rs1 = (uint8_t) binary_to_decimal(rs1_binary);

    uint8_t func3;
    char func3_binary[4];
    assign_format_binary(instruction->binary, 17, func3_binary, 4);
    func3 = (uint8_t) binary_to_decimal(func3_binary);

    uint8_t rd;
    char rd_binary[6];
    assign_format_binary(instruction->binary, 20, rd_binary, 6);
    rd = (uint8_t) binary_to_decimal(rd_binary);

    if (instruction->opcode == 0b0110011) {
        // Logic and arithmetic
        if (func3 == 0b000 && func7 == 0b0000000) {
            add(vm, rd, rs1, rs2);
        } else if (func3 == 0b000 && func7 == 0b0100000) {
            sub(vm, rd, rs1, rs2);
        } else if (func3 == 0b100 && func7 == 0b0000000) {
            xor(vm, rd, rs1, rs2);
        } else if (func3 == 0b110 && func7 == 0b0000000) {
            or(vm, rd, rs1, rs2);
        } else if (func3 == 0b111 && func7 == 0b0000000) {
            and(vm, rd, rs1, rs2);
        } else if (func3 == 0b001 && func7 == 0b0000000) {
            sll(vm, rd, rs1, rs2);
        } else if (func3 == 0b101 && func7 == 0b0000000) {
            srl(vm, rd, rs1, rs2);
        } else if (func3 == 0b101 && func7 == 0b0100000) {
            sra(vm, rd, rs1, rs2);
        // Program flow
        } else if (func3 == 0b010 && func7 == 0b0000000) {
            slt(vm, rd, rs1, rs2);
        } else if (func3 == 0b011 && func7 == 0b0000000) {
            sltu(vm, rd, rs1, rs2);
        }
		vm->program_counter += 4;
    }
}

int set_imm_sign(char* imm_binary) {
	char sign = convert_twos_complement(imm_binary);
    int imm = binary_to_decimal(imm_binary);
    if (sign == '-') {
        imm *= -1;
    }
	return imm;
}

void execute_I(VirtualMachine* vm, Instruction* instruction) {
    char imm_binary[13];
    assign_format_binary(instruction->binary, 0, imm_binary, 13);
    int imm = set_imm_sign(imm_binary);

    uint8_t rs1;
    char rs1_binary[6];
    assign_format_binary(instruction->binary, 12, rs1_binary, 6);
    rs1 = (uint8_t) binary_to_decimal(rs1_binary);

    uint8_t func3;
    char func3_binary[4];
    assign_format_binary(instruction->binary, 17, func3_binary, 4);
    func3 = (uint8_t) binary_to_decimal(func3_binary);

    uint8_t rd;
    char rd_binary[6];
    assign_format_binary(instruction->binary, 20, rd_binary, 6);
    rd = (uint8_t) binary_to_decimal(rd_binary);

    // Logic and arithmetic
    if (instruction->opcode == 0b0010011) {
        if (func3 == 0b000) {
            addi(vm, rd, rs1, imm);
		} else if (func3 == 0b100) {
            xori(vm, rd, rs1, imm);
        } else if (func3 == 0b110) {
            ori(vm, rd, rs1, imm);
        } else if (func3 == 0b111) {
            andi(vm, rd, rs1, imm);
        // Program flow
        } else if (func3 == 0b010) {
            slti(vm, rd, rs1, imm);
        } else if (func3 == 0b011) {
            sltiu(vm, rd, rs1, imm);
        }
		vm->program_counter += 4;
    // Memory
    } else if (instruction->opcode == 0b0000011) {
        if (func3 == 0b000) {
            lb(vm, rd, rs1, imm);
        } else if (func3 == 0b001) {
            lh(vm, rd, rs1, imm);
        } else if (func3 == 0b010) {
            lw(vm, rd, rs1, imm);
        } else if (func3 == 0b100) {
            lbu(vm, rd, rs1, imm);
        } else if (func3 == 0b101) {
            lhu(vm, rd, rs1, imm);
        }
		vm->program_counter += 4;
    // Program flow
    } else if (instruction->opcode == 0b1100111) {
        if (func3 == 0b000) {
            jalr(vm, rd, rs1, imm);
        }
    }
}

void execute_S(VirtualMachine* vm, Instruction* instruction) {
    char imm_binary[13];	
    assign_format_binary(instruction->binary, 0, imm_binary, 7+1);
    assign_format_binary(instruction->binary , 20, imm_binary + 7, 5+1);
    int imm = set_imm_sign(imm_binary);

    uint8_t rs2;
    char rs2_binary[6];
    assign_format_binary(instruction->binary, 7, rs2_binary, 6);
    rs2 = (uint8_t) binary_to_decimal(rs2_binary);

    uint8_t rs1;
    char rs1_binary[6];
    assign_format_binary(instruction->binary, 12, rs1_binary, 6);
    rs1 = (uint8_t) binary_to_decimal(rs1_binary);

    uint8_t func3;
    char func3_binary[4];
    assign_format_binary(instruction->binary, 17, func3_binary, 4);
    func3 = (uint8_t) binary_to_decimal(func3_binary);

    if (instruction->opcode == 0b0100011) {
        if (func3 == 0b000) {
            sb(vm, rs1, imm ,rs2);
        } else if (func3 == 0b001) {
            sh(vm, rs1, imm ,rs2);
        } else if (func3 == 0b010) {
            sw(vm, rs1, imm ,rs2);
        }
		vm->program_counter += 4;
    }
}

void execute_SB(VirtualMachine* vm, Instruction* instruction) {
    char imm_binary[13];	
    assign_format_binary(instruction->binary, 0, imm_binary, 1+1);
    assign_format_binary(instruction->binary, 24, imm_binary + 1, 1+1);
    assign_format_binary(instruction->binary, 1, imm_binary + 2, 6+1);
    assign_format_binary(instruction->binary, 20, imm_binary + 8, 4+1);
	int imm = set_imm_sign(imm_binary);

    uint8_t rs2;
    char rs2_binary[6];
    assign_format_binary(instruction->binary, 7, rs2_binary, 6);
    rs2 = (uint8_t) binary_to_decimal(rs2_binary);

    uint8_t rs1;
    char rs1_binary[6];
    assign_format_binary(instruction->binary, 12, rs1_binary, 6);
    rs1 = (uint8_t) binary_to_decimal(rs1_binary);

    uint8_t func3;
    char func3_binary[4];
    assign_format_binary(instruction->binary, 17, func3_binary, 4);
    func3 = (uint8_t) binary_to_decimal(func3_binary);

    if (instruction->opcode == 0b1100011) {
        if (func3 == 0b000) {
            beq(vm, rs1, rs2, imm);
        } else if (func3 == 0b001) {
            bne(vm, rs1, rs2, imm);
        } else if (func3 == 0b100) {
            blt(vm, rs1, rs2, imm);
        } else if (func3 == 0b110) {
            bltu(vm, rs1, rs2, imm);
        } else if (func3 == 0b101) {
            bge(vm, rs1, rs2, imm);
        } else if (func3 == 0b111) {
            bgeu(vm, rs1, rs2, imm);
        }
    }
}

void execute_U(VirtualMachine* vm, Instruction* instruction) {
    char imm_binary[21];
    assign_format_binary(instruction->binary, 0, imm_binary, 21);
    int imm = set_imm_sign(imm_binary);

    uint8_t rd;
    char rd_binary[6];
    assign_format_binary(instruction->binary, 20, rd_binary, 6);
    rd = (uint8_t) binary_to_decimal(rd_binary);

    if (instruction->opcode == 0b0110111) {
        lui(vm, rd, imm);
		vm->program_counter += 4;
    }
}

void execute_UJ(VirtualMachine* vm, Instruction* instruction) {
    char imm_binary[21];
    assign_format_binary(instruction->binary, 0, imm_binary, 1+1);
    assign_format_binary(instruction->binary, 12, imm_binary + 1, 8+1);
    assign_format_binary(instruction->binary, 11, imm_binary + 9, 1+1);
    assign_format_binary(instruction->binary, 1, imm_binary + 10, 10+1);
	int imm = set_imm_sign(imm_binary);

    uint8_t rd;
    char rd_binary[6];
    assign_format_binary(instruction->binary, 20, rd_binary, 6);
    rd = (uint8_t) binary_to_decimal(rd_binary);

    if (instruction->opcode == 0b1101111) {
        jal(vm, rd, imm);
    }
}

// Execute instructions
int execute_instructions(VirtualMachine* vm) {
    while (1) {
        Instruction instruction;

        int num = vm->instruction_memory[vm->program_counter / 4];
        decimal_to_binary(instruction.binary, num);

        char opcode_binary[8];
        opcode_binary[7] = '\0';
        for (int j = 25; j <= 31; j++) {
            opcode_binary[j - 25] = instruction.binary[j];
        }
        instruction.opcode = (uint8_t) strtoul(opcode_binary, NULL, 2);

        if (instruction.opcode > 0) {
            get_instruction_type(&instruction);

            if (strcmp(instruction.type, "R") == 0) {
                execute_R(vm, &instruction);
            } else if (strcmp(instruction.type, "I") == 0) {
                execute_I(vm, &instruction);
            } else if (strcmp(instruction.type, "S") == 0) {
                execute_S(vm, &instruction);
            } else if (strcmp(instruction.type, "SB") == 0) {
                execute_SB(vm, &instruction);
            } else if (strcmp(instruction.type, "U") == 0) {
                execute_U(vm, &instruction);
            } else if (strcmp(instruction.type, "UJ") == 0) {
                execute_UJ(vm, &instruction);
            } else {
                fake_instruction(vm, num);
				return 1;
            }
        } else {
			fake_instruction(vm, num);
			return 1;
		}
    }	
	return 0;
}

int main(int argc, char* argv[]) {
    // Open file 
    char *file_path = argv[1];
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("error opening file");
        return 1;
    }

    // Initialise the VM
    VirtualMachine vm = {0, {0}};
	for (int i = 0; i < 128; i++) {
		HeapMemory new_memory;
		new_memory.first_bank = 255;
		new_memory.banks_used = 0;
		new_memory.value = 0;
		new_memory.next = 0;
		vm.heap_banks[i] = new_memory;
	}

    // Read binary file
    unsigned char c;
    unsigned int num = 0;
    int line_count = 0;
    int total_count = 0;

    while (fread(&c, sizeof(unsigned char), 1, file) == 1) {
        num = num | ((unsigned int)c << (line_count * 8));
        line_count++;

        if (line_count == 4) {
            // Store instruction to instruction_memory 
            if (total_count < 1024) {
                int index = (total_count) / 4;
                vm.instruction_memory[index] = num;
            } else {
                int index = (total_count - 1024) / 4;
                vm.data_memory[index] = num;
            }

            num = 0;
            line_count = 0;
        }

        total_count++;
    }

    int success = execute_instructions(&vm);

    fclose(file);

	return success;
}