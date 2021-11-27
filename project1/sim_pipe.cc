#include "sim_pipe.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>
#include <vector>

//#define DEBUG

using namespace std;

//used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {"PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP"};
int instr_num = 0;
unsigned latency_ctr = 0;
bool NOP_flag = false;
bool BRANCH_flag = false;
bool EOP_flag = false;
bool DMEM_flag = false;
instruction_t instr_NOP;


/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */


/* converts integer into array of unsigned char - little indian */
inline void int2char(unsigned value, unsigned char *buffer){
	memcpy(buffer, &value, sizeof value);
}

/* converts array of char into integer - little indian */
inline unsigned char2int(unsigned char *buffer){
	unsigned d;
	memcpy(&d, buffer, sizeof d);
	return d;
}

/* implements the ALU operations */
unsigned alu(unsigned opcode, unsigned a, unsigned b, unsigned imm, unsigned npc){
	switch(opcode){
			case ADD:
				return (a+b);
			case ADDI:
				return(a+imm);
			case SUB:
				return(a-b);
			case SUBI:
				return(a-imm);
			case XOR:
				return(a ^ b);
			case LW:
			case SW:
				return(a + imm);
			case BEQZ:
			case BNEZ:
			case BGTZ:
			case BGEZ:
			case BLTZ:
			case BLEZ:
			case JUMP:
				return(npc+imm);
			default:	
				return (-1);
	}
}

unsigned branchcond(unsigned opcode, unsigned a){
    switch(opcode){
        case BEQZ:
            return (a == 0);
        case BNEZ:
            return (a != 0);
        case BGTZ:
            return (a > 0);
        case BGEZ:
            return (a >= 0);
        case BLTZ:
            return (a < 0);
        case BLEZ:
            return (a <= 0);
        default:
            return (-1);
    }
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* loads the assembly program in file "filename" in instruction memory at the specified address */
void sim_pipe::load_program(const char *filename, unsigned base_address){

   /* initializing the base instruction address */
   instr_base_address = base_address;

   /* creating a map with the valid opcodes and with the valid labels */
   map<string, opcode_t> opcodes; //for opcodes
   map<string, unsigned> labels;  //for branches
   for (int i=0; i<NUM_OPCODES; i++)
	 opcodes[string(instr_names[i])]=(opcode_t)i;

   /* opening the assembly file */
   ifstream fin(filename, ios::in | ios::binary);
   if (!fin.is_open()) {
      cerr << "error: open file " << filename << " failed!" << endl;
      exit(-1);
   }

   /* parsing the assembly file line by line */
   string line;
   unsigned instruction_nr = 0;
   while (getline(fin,line)){
	// set the instruction field
	char *str = const_cast<char*>(line.c_str());

  	// tokenize the instruction
	char *token = strtok (str," \t");
	map<string, opcode_t>::iterator search = opcodes.find(token);
        if (search == opcodes.end()){
		// this is a label for a branch - extract it and save it in the labels map
		string label = string(token).substr(0, string(token).length() - 1);
		labels[label]=instruction_nr;
                // move to next token, which must be the instruction opcode
		token = strtok (NULL, " \t");
		search = opcodes.find(token);
		if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
	}
	instr_memory[instruction_nr].opcode = search->second;

	//reading remaining parameters
	char *par1;
	char *par2;
	char *par3;
	switch(instr_memory[instruction_nr].opcode){
		case ADD:
		case SUB:
		case XOR:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "R"));
			break;
		case ADDI:
		case SUBI:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].immediate = strtoul (par3, NULL, 0); 
			break;
		case LW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
			break;
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].label = par2;
			break;
		case JUMP:
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].label = par2;
		default:
			break;

	} 

	/* increment instruction number before moving to next line */
	instruction_nr++;
   }
   //reconstructing the labels of the branch operations
   int i = 0;
   while(true){
   	instruction_t instr = instr_memory[i];
	if (instr.opcode == EOP) break;
	if (instr.opcode == BLTZ || instr.opcode == BNEZ ||
            instr.opcode == BGTZ || instr.opcode == BEQZ ||
            instr.opcode == BGEZ || instr.opcode == BLEZ ||
            instr.opcode == JUMP
	 ){
		instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
	}
        i++;
   }
    IF_ID.PC = instr_base_address;
}

/* writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness) */
void sim_pipe::write_memory(unsigned address, unsigned value){
	int2char(value,data_memory+address);
}

/* prints the content of the data memory within the specified address range */
void sim_pipe::print_memory(unsigned start_address, unsigned end_address){
	cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": "; 
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3) cout << endl;
	} 
}

/* prints the values of the registers */
void sim_pipe::print_registers(){
        cout << "Special purpose registers:" << endl;
        unsigned i, s;
        for (s=0; s<NUM_STAGES; s++){
                cout << "Stage: " << stage_names[s] << endl;
                for (i=0; i< NUM_SP_REGISTERS; i++)
                        if ((sp_register_t)i != IR && (sp_register_t)i != COND && get_sp_register((sp_register_t)i, (stage_t)s)!=UNDEFINED)
                            cout << reg_names[i] << " = " << dec <<  get_sp_register((sp_register_t)i, (stage_t)s)
                            << hex << " / 0x" << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
        }
        cout << "General purpose registers:" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++)
                if (get_gp_register(i)!=(int)UNDEFINED) cout << "R" << dec << i << " = " << get_gp_register(i) << hex << " / 0x" << get_gp_register(i) << endl;
}

/* initializes the pipeline simulator */
sim_pipe::sim_pipe(unsigned mem_size, unsigned mem_latency){
	data_memory_size = mem_size;
	data_memory_latency = mem_latency;
	data_memory = new unsigned char[data_memory_size];
	reset();
}
	
/* deallocates the pipeline simulator */
sim_pipe::~sim_pipe(){
	delete [] data_memory;
}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */


/* body of the simulator */
void sim_pipe::run(unsigned cycles){
    unsigned i;
    instruction_t instruction;
    instr_NOP.opcode = NOP;
    char finish_eop = 0;
    if(cycles == 0){
        cycles = 250; //enough to run all cases (if eop is in WB stage the for loop breaks)
    }

    for(i = 0; i < cycles; i++){
        // WB Stage
        if(!MEMORY.empty()) {
            if (WRITEBACK.empty()) {
                if(DMEM_flag){
                    WRITEBACK.push_back(instr_NOP);
                }
                else {
                    WRITEBACK.push_back(MEMORY.back());
                    MEMORY.pop_back();
                }
            }
            instruction = WRITEBACK.back();

            WRITEBACK.pop_back();
            switch (instruction.opcode) {
                case LW:
                    set_gp_register(instruction.dest, MEM_WB.LMD);
                    del_RAW(instruction.dest);
                case SW:
                    latency_ctr = data_memory_latency;
                    num_inst++;
                    break;
                case ADD:
                case ADDI:
                case SUB:
                case SUBI:
                case XOR:
                    num_inst++;
                    set_gp_register(instruction.dest, MEM_WB.ALU_OUTPUT);
                    del_RAW(instruction.dest);
                    break;
                case EOP:
                    finish_eop = 2;
                    break;
                case NOP:
                    num_stall++;
                    break;
                default:
                    num_inst++;
                    break;
            }
        }
        if(finish_eop == 2) break;

        // MEM Stage
        if(!EXECUTE.empty()) {
            if (MEMORY.empty()) {
                MEMORY.push_back(EXECUTE.back());
                EXECUTE.pop_back();
            }

            instruction = MEMORY.back();
            switch(instruction.opcode){
                case LW:
                case ADD:
                case ADDI:
                case SUB:
                case SUBI:
                case XOR:
                    addRAW(instruction.dest);
                    break;
                default:
                    break;
            }

            switch (instruction.opcode) {
                case LW:
                    if(latency_ctr > 0) {
                        DMEM_flag = true;
                        latency_ctr--;
                    }
                    else{
                        DMEM_flag = false;
                        MEM_WB.LMD = read_memory(EXE_MEM.ALU_OUTPUT);
                        MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT;
                    }

                  /*  MEM_WB.LMD = read_memory(EXE_MEM.ALU_OUTPUT);
                    MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT; */
                    break;
                case SW:
                    if(latency_ctr > 0){
                        DMEM_flag = true;
                        latency_ctr--;
                    }
                    else{
                        DMEM_flag = false;
                        write_memory(EXE_MEM.ALU_OUTPUT, EXE_MEM.B);
                        MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT;
                        MEM_WB.LMD = UNDEFINED;
                    }

                 /*   write_memory(EXE_MEM.ALU_OUTPUT, EXE_MEM.B);
                    MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT;
                    MEM_WB.LMD = UNDEFINED; */
                    break;
                case ADD:
                case ADDI:
                case SUB:
                case SUBI:
                case XOR:
                    MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT;
                    MEM_WB.LMD = UNDEFINED;
                    break;
                case BEQZ:
                case BNEZ:
                case BLTZ:
                case BGTZ:
                case BLEZ:
                case BGEZ:
                    MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT;
                    BRANCH_flag = false;
                    if(EXE_MEM.COND) {
                        IF_ID.PC = EXE_MEM.ALU_OUTPUT;
                        instr_num = (EXE_MEM.ALU_OUTPUT - instr_base_address)/4;
                        if(EOP_flag) {
                            FETCH.clear();
                            EOP_flag = false;
                            finish_eop = 0;
                        }
                    }
                    else{
                        IF_ID.NPC = IF_ID.PC;
                    }
                    break;
                case JUMP:
                    MEM_WB.ALU_OUTPUT = EXE_MEM.ALU_OUTPUT;
                    BRANCH_flag = false;
                    IF_ID.PC = EXE_MEM.ALU_OUTPUT;
                    instr_num = (EXE_MEM.ALU_OUTPUT - instr_base_address)/4;
                    if(EOP_flag) {
                        FETCH.clear();
                        EOP_flag = false;
                        finish_eop = 0;
                    }
                    break;
                case NOP:
                case EOP:
                    MEM_WB.ALU_OUTPUT = UNDEFINED;
                    MEM_WB.LMD = UNDEFINED;
                    break;
                default:
                    break;
            }
        }

        //EXE Stage
        if(!DECODE.empty()) {
            if (EXECUTE.empty()) {
                if(NOP_flag){
                    EXECUTE.push_back(instr_NOP);
                }
                else {
                    EXECUTE.push_back(DECODE.back());
                    DECODE.pop_back();
                }
            }
            if(!DMEM_flag) {
                instruction = EXECUTE.back();
                switch (instruction.opcode) {
                    case LW:
                    case ADD:
                    case ADDI:
                    case SUB:
                    case SUBI:
                    case XOR:
                        addRAW(instruction.dest);
                        break;
                    default:
                        break;
                }

                switch (instruction.opcode) {
                    case LW:
                        EXE_MEM.ALU_OUTPUT = ID_EXE.A + ID_EXE.IMM;
                        EXE_MEM.B = UNDEFINED;
                        break;
                    case SW:
                        EXE_MEM.ALU_OUTPUT = ID_EXE.A + ID_EXE.IMM;
                        EXE_MEM.B = ID_EXE.B;
                        break;
                    case ADD:
                    case SUB:
                    case XOR:
                        EXE_MEM.ALU_OUTPUT = alu(instruction.opcode, ID_EXE.A, ID_EXE.B, ID_EXE.IMM, ID_EXE.NPC);
                        EXE_MEM.B = ID_EXE.B;
                        break;
                    case ADDI:
                    case SUBI:
                        EXE_MEM.ALU_OUTPUT = alu(instruction.opcode, ID_EXE.A, ID_EXE.B, ID_EXE.IMM, ID_EXE.NPC);
                        EXE_MEM.B = UNDEFINED;
                        break;
                    case NOP:
                    case EOP:
                        EXE_MEM.ALU_OUTPUT = UNDEFINED;
                        EXE_MEM.B = UNDEFINED;
                        break;
                    case BEQZ:
                    case BNEZ:
                    case BLTZ:
                    case BGTZ:
                    case BLEZ:
                    case BGEZ:
                        EXE_MEM.ALU_OUTPUT = ID_EXE.NPC + (ID_EXE.IMM);
                        BRANCH_flag = true;
                        EXE_MEM.COND = branchcond(instruction.opcode, ID_EXE.A);
                        FETCH.push_back(instr_NOP);
                        break;
                    case JUMP:
                        EXE_MEM.ALU_OUTPUT = ID_EXE.NPC + (ID_EXE.IMM);
                        BRANCH_flag = true;
                        FETCH.push_back(instr_NOP);
                        break;
                    default:
                        break;
                }
            }
        }

        //ID Stage
        if(!FETCH.empty()) {
            if (DECODE.empty()) {
                DECODE.push_back(FETCH.back());
                FETCH.pop_back();
            }


                instruction = DECODE.back();
            if(!DMEM_flag) {
                switch (instruction.opcode) {
                    case SW:
                    case ADD:
                    case SUB:
                    case XOR:
                        if (checkRAW(instruction.src2)) {
                            NOP_flag = true;
                            break;
                        }
                    case LW:
                    case ADDI:
                    case SUBI:
                    case BEQZ:
                    case BNEZ:
                    case BLTZ:
                    case BGTZ:
                    case BLEZ:
                    case BGEZ:
                        if (checkRAW(instruction.src1)) {
                            NOP_flag = true;
                            break;
                        }
                        NOP_flag = false;
                        break;
                    default:
                        break;
                }


                switch (instruction.opcode) {
                    case LW:
                    case ADD:
                    case ADDI:
                    case SUB:
                    case SUBI:
                    case XOR:
                        addRAW(instruction.dest);
                        break;
                    default:
                        break;
                }

                if (NOP_flag) {
                    instruction = instr_NOP;
                }
            }
            if(!DMEM_flag || (instruction.opcode >= BEQZ && instruction.opcode <= JUMP)) {
                switch (instruction.opcode) {
                    case LW:
                        ID_EXE.A = get_gp_register(instruction.src1);
                        ID_EXE.B = UNDEFINED;
                        ID_EXE.IMM = instruction.immediate;
                        ID_EXE.NPC = IF_ID.NPC;
                        break;
                    case SW:
                        ID_EXE.A = get_gp_register(instruction.src2);
                        ID_EXE.B = get_gp_register(instruction.src1);
                        ID_EXE.IMM = instruction.immediate;
                        ID_EXE.NPC = IF_ID.NPC;
                        break;
                    case ADD:
                    case SUB:
                    case XOR:
                        ID_EXE.A = get_gp_register(instruction.src1);
                        ID_EXE.B = get_gp_register(instruction.src2);
                        ID_EXE.IMM = UNDEFINED;
                        ID_EXE.NPC = IF_ID.NPC;
                        break;
                    case ADDI:
                    case SUBI:
                        ID_EXE.A = get_gp_register(instruction.src1);
                        ID_EXE.B = UNDEFINED;
                        ID_EXE.IMM = instruction.immediate;
                        ID_EXE.NPC = IF_ID.NPC;
                        break;
                    case NOP:
                        ID_EXE.NPC = UNDEFINED;
                        ID_EXE.A = UNDEFINED;
                        ID_EXE.B = UNDEFINED;
                        ID_EXE.IMM = UNDEFINED;
                        break;
                    case EOP:
                        ID_EXE.NPC = IF_ID.NPC;
                        ID_EXE.A = UNDEFINED;
                        ID_EXE.B = UNDEFINED;
                        ID_EXE.IMM = UNDEFINED;
                        break;
                    case BEQZ:
                    case BNEZ:
                    case BLTZ:
                    case BGTZ:
                    case BLEZ:
                    case BGEZ:
                    case JUMP:
                        if(!DMEM_flag) {
                            ID_EXE.A = get_gp_register(instruction.src1);
                            ID_EXE.B = UNDEFINED;
                            ID_EXE.IMM = instruction.immediate;
                            ID_EXE.NPC = IF_ID.NPC;
                        }
                        BRANCH_flag = true;
                        FETCH.clear();
                        FETCH.push_back(instr_NOP);
                        break;
                    default:
                        break;
                }
            }

        }

        //IF Stage
        instruction = instr_memory[instr_num];
        if(FETCH.empty() && !EOP_flag){
            switch(instruction.opcode){
                case EOP:
                    if(!finish_eop)
                    FETCH.insert(FETCH.begin(), instruction);
                    EOP_flag = true;
                    finish_eop = 1;
                case NOP:
                    break;
                default:
                    instr_num = instr_num + 1;
                    FETCH.insert(FETCH.begin(), instruction);
                    if((!NOP_flag)) {
                        IF_ID.NPC = IF_ID.PC + 4;
                        IF_ID.PC += 4;
                    }
                    break;
            }

        }
        else if(!DMEM_flag) {
             if (!EOP_flag) {
                instruction = FETCH.back();
                switch (instruction.opcode) {
                    case NOP:
                        if (BRANCH_flag) IF_ID.NPC = UNDEFINED;
                    case EOP:
                        break;
                    default:
                        if ((!NOP_flag)) {
                            IF_ID.NPC = IF_ID.PC + 4;
                            IF_ID.PC += 4;
                        }
                        break;
                }
            }
        }
    }
}
	
/* reset the state of the pipeline simulator */
void sim_pipe::reset(){
    initialize_gp_reg();
    initialize_data_mem();
    num_clock = 0;
    num_inst = 0;
    num_stall = 0;
    latency_ctr = data_memory_latency;

}

//return value of special purpose register
unsigned sim_pipe::get_sp_register(sp_register_t reg, stage_t s){
    switch(s){
        case IF:     // IF Stage
        if(reg == PC){
            return IF_ID.PC;
        }
        else{
            return UNDEFINED;
        }
        case ID:     // ID Stage
            if(reg == NPC){
                return IF_ID.NPC;
            }
            else{
                return UNDEFINED;
            }
        case EXE:     // EXE Stage
            switch(reg){
                case 1:     // NPC
                    return ID_EXE.NPC;
                case 3:     // A
                    return ID_EXE.A;
                case 4:     // B
                    return ID_EXE.B;
                case 5:     // IMM
                    return ID_EXE.IMM;
                default:
                    return UNDEFINED;
            }
        case MEM:     // MEM Stage
            switch(reg){
                case 4:     // B
                    return EXE_MEM.B;
                case 7:     // ALU_OUTPUT
                    return EXE_MEM.ALU_OUTPUT;
                default:
                    return UNDEFINED;
            }
        case WB:     // WB Stage
            switch(reg){
                case 7:     // ALU_OUTPUT
                    return MEM_WB.ALU_OUTPUT;
                case 8:     // LMD
                    return MEM_WB.LMD;
                default:
                    return UNDEFINED;
            }
        default:
            return 0;
    }
}

//returns value of general purpose register
int sim_pipe::get_gp_register(unsigned reg){
	return gp_register[reg]; //please modify
}

//sets gp register to a value
void sim_pipe::set_gp_register(unsigned reg, int value){
    gp_register[reg] = value;
}

//returns the IPC
float sim_pipe::get_IPC(){
    if(num_inst == 0) return 0;
    num_IPC = num_inst/num_clock;
    return num_IPC;
}

//returns number of instructions completed
unsigned sim_pipe::get_instructions_executed(){
        return num_inst;
}

unsigned sim_pipe::get_stalls(){
        return num_stall;
}

//returns number of clock cycles completed
unsigned sim_pipe::get_clock_cycles(){
    if(num_inst != 0){
        num_clock = num_inst + num_stall + 4; // clock cycles equation
    }
        return num_clock;
}

//returns 4 bytes from data memory read in little endian
unsigned sim_pipe::read_memory(unsigned address) {
    return char2int(data_memory+address);
}

//checks to see if the register is being written
bool sim_pipe::checkRAW(unsigned reg) {
    return RAW[reg];
}

//adds a register that will be written
void sim_pipe::addRAW(unsigned reg) {
    RAW[reg] = true;
}

//register has been written thus removed
void sim_pipe::del_RAW(unsigned reg){
    RAW[reg] = false;
}
//Alan Zheng ECE 463 3/4/2021