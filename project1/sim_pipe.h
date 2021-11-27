#ifndef SIM_PIPE_H_
#define SIM_PIPE_H_

#include <stdio.h>
#include <string>
#include <vector>

using namespace std;

#define PROGRAM_SIZE 50

#define UNDEFINED 0xFFFFFFFF //used to initialize the registers
#define NUM_SP_REGISTERS 9
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 16 
#define NUM_STAGES 5

typedef enum {PC, NPC, IR, A, B, IMM, COND, ALU_OUTPUT, LMD} sp_register_t;

typedef enum {LW, SW, ADD, ADDI, SUB, SUBI, XOR, BEQZ, BNEZ, BLTZ, BGTZ, BLEZ, BGEZ, JUMP, EOP, NOP} opcode_t;
/*
                case LW:
                case SW:
                case ADD:
                case ADDI:
                case SUB:
                case SUBI:
                case XOR:
                case BEQZ:
                case BNEZ:
                case BLTZ:
                case BGTZ:
                case BLEZ:
                case BGEZ:
                case JUMP:
                case EOP:
                case NOP:
                default:
 */
typedef enum {IF, ID, EXE, MEM, WB} stage_t;

typedef struct{
        opcode_t opcode; //opcode
        unsigned src1; //first source register in the assembly instruction (for SW, register to be written to memory) (rs)
        unsigned src2; //second source register in the assembly instruction (rt)
        unsigned dest; //destination register (rd)
        unsigned immediate; //immediate field
        string label; //for conditional branches, label of the target instruction - used only for parsing/debugging purposes
} instruction_t;

typedef struct{
    unsigned PC = UNDEFINED;
    unsigned NPC = UNDEFINED;
    unsigned A = UNDEFINED;
    unsigned B = UNDEFINED;
    unsigned IMM = UNDEFINED;
    unsigned COND = UNDEFINED;
    unsigned ALU_OUTPUT = UNDEFINED;
    unsigned LMD = UNDEFINED;
} pipeline_register;

class sim_pipe{

	/* Add the data members required by your simulator's implementation here */

	//array of gp_register which represents R0 to R31 (32 total registers)
    unsigned gp_register [NUM_GP_REGISTERS];

    //floats to keep track of number of clock cycles, instructions completed, stalls implemented, and Instructions per Clock (IPC)
    float num_clock, num_inst, num_stall, num_IPC;

    //used to hold simulate the pipeline registers which holds and moves data from each stages
    pipeline_register IF_ID;
    pipeline_register ID_EXE;
    pipeline_register EXE_MEM;
    pipeline_register MEM_WB;

    //used to hold instructions at each stage of the 5 stages
    vector<instruction_t> FETCH;
    vector<instruction_t> DECODE;
    vector<instruction_t> EXECUTE;
    vector<instruction_t> MEMORY;
    vector<instruction_t> WRITEBACK;

    //Read after Write array which is set false initially for all registers
    unsigned RAW[NUM_GP_REGISTERS];

    //instruction memory
    instruction_t instr_memory[PROGRAM_SIZE];

    //base address in the instruction memory where the program is loaded
    unsigned instr_base_address;

	//data memory - should be initialize to all 0xFF
	unsigned char *data_memory;

	//memory size in bytes
	unsigned data_memory_size;
	
	//memory latency in clock cycles
	unsigned data_memory_latency;

public:

	//instantiates the simulator with a data memory of given size (in bytes) and latency (in clock cycles)
	/* Note: 
           - initialize the registers to UNDEFINED value 
	   - initialize the data memory to all 0xFF values
	*/

	sim_pipe(unsigned data_mem_size, unsigned data_mem_latency);
	
	//de-allocates the simulator
	~sim_pipe();

	//loads the assembly program in file "filename" in instruction memory at the specified address
	void load_program(const char *filename, unsigned base_address=0x0);

	//runs the simulator for "cycles" clock cycles (run the program to completion if cycles=0) 
	void run(unsigned cycles=0);
	
	//resets the state of the simulator
        /* Note: 
	   - registers should be reset to UNDEFINED value 
	   - data memory should be reset to all 0xFF values
	*/
	void reset();

	// returns value of the specified special purpose register for a given stage (at the "entrance" of that stage)
        // if that special purpose register is not used in that stage, returns UNDEFINED
        //
        // Examples (refer to page C-37 in the 5th edition textbook, A-32 in 4th edition of textbook)::
        // - get_sp_register(PC, IF) returns the value of PC
        // - get_sp_register(NPC, ID) returns the value of IF/ID.NPC
        // - get_sp_register(NPC, EX) returns the value of ID/EX.NPC
        // - get_sp_register(ALU_OUTPUT, MEM) returns the value of EX/MEM.ALU_OUTPUT
        // - get_sp_register(ALU_OUTPUT, WB) returns the value of MEM/WB.ALU_OUTPUT
	// - get_sp_register(LMD, ID) returns UNDEFINED
	/* Note: you are allowed to use a custom format for the IR register.
           Therefore, the test cases won't check the value of IR using this method. 
	   You can add an extra method to retrieve the content of IR */
	unsigned get_sp_register(sp_register_t reg, stage_t stage);

	//returns value of the specified general purpose register
	int get_gp_register(unsigned reg);

	// set the value of the given general purpose register to "value"
	void set_gp_register(unsigned reg, int value);

	//returns the IPC
	float get_IPC();

	//returns the number of instructions fully executed
	unsigned get_instructions_executed();

	//returns the number of clock cycles 
	unsigned get_clock_cycles();

	//returns the number of stalls added by processor
	unsigned get_stalls();

	//prints the content of the data memory within the specified address range
	void print_memory(unsigned start_address, unsigned end_address);

	//writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness)
	void write_memory(unsigned address, unsigned value);

	//reads and returns a integer value in data memory at a specific address
	unsigned read_memory(unsigned address);

	//prints the values of the registers 
	void print_registers();

	//function used to initialize gp registers and Read After Write checker
	void initialize_gp_reg(){
	    for(unsigned i = 0; i < NUM_GP_REGISTERS; i++){
            gp_register[i] = UNDEFINED;
            RAW[i] = false;
	    }
	}

	//function used to initialize data memory to 0xFF from 0 to the data_memory_size
	void initialize_data_mem(){
	    unsigned i, j;
        for (i = 0x0, j= 0xFF; i<data_memory_size; i+=1) write_memory(i,j);
	}

	//checks a register in the RAW array for true or false
	bool checkRAW(unsigned reg);

	//add a register into the RAW array
	void addRAW(unsigned reg);

	//removes a register from RAW array
	void del_RAW(unsigned reg);
};

#endif /*SIM_PIPE_H_*/

//Alan Zheng ECE 463 3/4/2021