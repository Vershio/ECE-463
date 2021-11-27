//-------------------------------------
//      ECE 463 Project 2
//          Alan Zheng
//      NCSU Spring 2021
//-------------------------------------
#include "sim_ooo.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>

using namespace std;

//used for debugging purposes
static const char *stage_names[NUM_STAGES] = {"ISSUE", "EXE", "WR", "COMMIT"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "AND", "MULT", "DIV", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "LWS", "SWS", "ADDS", "SUBS", "MULTS", "DIVS"};
static const char *res_station_names[5]={"Int", "Add", "Mult", "Load"};

/* =============================================================

   HELPER FUNCTIONS (misc)

   ============================================================= */


/* convert a float into an unsigned */
inline unsigned float2unsigned(float value){
	unsigned result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert an unsigned into a float */
inline float unsigned2float(unsigned value){
	float result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert integer into array of unsigned char - little indian */
inline void unsigned2char(unsigned value, unsigned char *buffer){
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
}

/* convert array of char into integer - little indian */
inline unsigned char2unsigned(unsigned char *buffer){
       return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/* the following six functions return the kind of the considered opcode */

bool is_branch(opcode_t opcode){
        return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ ||
        opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode){
        return (opcode == LW || opcode == SW || opcode == LWS || opcode == SWS);
}

bool is_int_r(opcode_t opcode){
        return (opcode == ADD || opcode == SUB || opcode == XOR || opcode == AND);
}

bool is_int_imm(opcode_t opcode){
        return (opcode == ADDI || opcode == SUBI );
}

bool is_int(opcode_t opcode){
        return (is_int_r(opcode) || is_int_imm(opcode));
}

bool is_fp_alu(opcode_t opcode){
        return (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS);
}

/* clears a ROB entry */
void clean_rob(rob_entry_t *entry){
        entry->ready=false;
        entry->pc=UNDEFINED;
        entry->state=ISSUE;
        entry->destination=UNDEFINED;
        entry->value=UNDEFINED;
}

/* clears a reservation station */
void clean_res_station(res_station_entry_t *entry){
        entry->pc=UNDEFINED;
        entry->value1=UNDEFINED;
        entry->value2=UNDEFINED;
        entry->tag1=UNDEFINED;
        entry->tag2=UNDEFINED;
        entry->destination=UNDEFINED;
        entry->address=UNDEFINED;

        entry->immediate = UNDEFINED;
        entry->ready = false;
        entry->executing = false;
}

/* clears an entry if the instruction window */
void clean_instr_window(instr_window_entry_t *entry){
        entry->pc=UNDEFINED;
        entry->issue=UNDEFINED;
        entry->exe=UNDEFINED;
        entry->wr=UNDEFINED;
        entry->commit=UNDEFINED;
}

/* implements the ALU operation
   NOTE: this function does not cover LOADS and STORES!
*/
unsigned alu(opcode_t opcode, unsigned value1, unsigned value2, unsigned immediate, unsigned pc){
	unsigned result;
	switch(opcode){
        case ADD:
        //case ADDI: wrong?? where is immediate
            result = value1 + value2;
            break;
	    case ADDI:
	        result = value1 + immediate;
	        break;
        case SUB:
        //case SUBI: not value 1 - value 2????
            result = value1 - value2;
            break;
	    case SUBI:
	        result = value1 - immediate;
	        break;
        case XOR:
            result = value1 ^ value2;
            break;
        case AND:
            result = value1 & value2;
            break;
        case MULT:
            result = value1 * value2;
            break;
        case DIV:
            result = value1 / value2;
            break;
        case ADDS:
            result = float2unsigned(unsigned2float(value1) + unsigned2float(value2));
            break;
        case SUBS:
            result = float2unsigned(unsigned2float(value1) - unsigned2float(value2));
            break;
        case MULTS:
            result = float2unsigned(unsigned2float(value1) * unsigned2float(value2));
            break;
        case DIVS:
            result = float2unsigned(unsigned2float(value1) / unsigned2float(value2));
            break;
        case JUMP:
            result = pc + 4 + immediate;
            break;
        default: //branches
            int reg = (int) value1;
            bool condition = ((opcode == BEQZ && reg==0) ||
            (opcode == BNEZ && reg!=0) ||
            (opcode == BGEZ && reg>=0) ||
            (opcode == BLEZ && reg<=0) ||
            (opcode == BGTZ && reg>0) ||
            (opcode == BLTZ && reg<0));
            if (condition)
                result = pc+4+immediate;
            else
                result = pc+4;
            break;
	}
	return result;
}

/* writes the data memory at the specified address */
void sim_ooo::write_memory(unsigned address, unsigned value){
	unsigned2char(value,data_memory+address);
}

/* =============================================================

   Handling of FUNCTIONAL UNITS

   ============================================================= */

/* initializes an execution unit */
void sim_ooo::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances){
        for (unsigned i=0; i<instances; i++){
                exec_units[num_units].type = exec_unit;
                exec_units[num_units].latency = latency;
                exec_units[num_units].busy = 0;
                exec_units[num_units].pc = UNDEFINED;
                num_units++;
        }
}

/* returns a free unit for that particular operation or UNDEFINED if no unit is currently available */
unsigned sim_ooo::get_free_unit(opcode_t opcode){
	if (num_units == 0){
		cout << "ERROR:: simulator does not have any execution units!\n";
		exit(-1);
	}
	for (unsigned u=0; u<num_units; u++){
		switch(opcode){
			//Integer unit
			case ADD:
			case ADDI:
			case SUB:
			case SUBI:
			case XOR:
			case AND:
			case BEQZ:
			case BNEZ:
			case BLTZ:
			case BGTZ:
			case BLEZ:
			case BGEZ:
			case JUMP:
				if (exec_units[u].type==INTEGER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			//memory unit
			case LW:
			case SW:
			case LWS:
			case SWS:
				if (exec_units[u].type==MEMORY && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			// FP adder
			case ADDS:
			case SUBS:
				if (exec_units[u].type==ADDER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			// Multiplier
			case MULT:
			case MULTS:
				if (exec_units[u].type==MULTIPLIER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			// Divider
			case DIV:
			case DIVS:
				if (exec_units[u].type==DIVIDER && exec_units[u].busy==0 && exec_units[u].pc==UNDEFINED) return u;
				break;
			default:
				cout << "ERROR:: operations not requiring exec unit!\n";
				exit(-1);
		}
	}
	return UNDEFINED;
}



/* ============================================================================

   Primitives used to print out the state of each component of the processor:
   	- registers
	- data memory
	- instruction window
        - reservation stations and load buffers
        - (cycle-by-cycle) execution log
	- execution statistics (CPI, # instructions executed, # clock cycles)

   =========================================================================== */


/* prints the content of the data memory */
void sim_ooo::print_memory(unsigned start_address, unsigned end_address){
	cout << "DATA MEMORY[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3){
			cout << endl;
		}
	}
}

/* prints the value of the registers */
void sim_ooo::print_registers(){
        unsigned i;
	cout << "GENERAL PURPOSE REGISTERS" << endl;
	cout << setfill(' ') << setw(8) << "Register" << setw(22) << "Value" << setw(5) << "ROB" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++){
                if (get_int_register_tag(i)!=UNDEFINED)
			cout << setfill(' ') << setw(7) << "R" << dec << i << setw(22) << "-" << setw(5) << get_int_register_tag(i) << endl;
                else if (get_int_register(i)!=(int)UNDEFINED)
			cout << setfill(' ') << setw(7) << "R" << dec << i << setw(11) << get_int_register(i) << hex << "/0x" << setw(8) << setfill('0') << get_int_register(i) << setfill(' ') << setw(5) << "-" << endl;
        }
	for (i=0; i< NUM_GP_REGISTERS; i++){
                if (get_fp_register_tag(i)!=UNDEFINED)
			cout << setfill(' ') << setw(7) << "F" << dec << i << setw(22) << "-" << setw(5) << get_fp_register_tag(i) << endl;
                else if (float2unsigned(get_fp_register(i))!=UNDEFINED)
			cout << setfill(' ') << setw(7) << "F" << dec << i << setw(11) << get_fp_register(i) << hex << "/0x" << setw(8) << setfill('0') << float2unsigned(get_fp_register(i)) << setfill(' ') << setw(5) << "-" << endl;
	}
	cout << endl;
}

/* prints the content of the ROB */
void sim_ooo::print_rob(){
	cout << "REORDER BUFFER" << endl;
	cout << setfill(' ') << setw(5) << "Entry" << setw(6) << "Busy" << setw(7) << "Ready" << setw(12) << "PC" << setw(10) << "State" << setw(6) << "Dest" << setw(12) << "Value" << endl;
	for(unsigned i=0; i< rob.num_entries;i++){
		rob_entry_t entry = rob.entries[i];
		instruction_t instruction;
		if (entry.pc != UNDEFINED) instruction = instr_memory[(entry.pc-instr_base_address)>>2];
		cout << setfill(' ');
		cout << setw(5) << i;
		cout << setw(6);
		if (entry.pc==UNDEFINED) cout << "no"; else cout << "yes";
		cout << setw(7);
		if (entry.ready) cout << "yes"; else cout << "no";
		if (entry.pc!= UNDEFINED ) cout << "  0x" << hex << setfill('0') << setw(8) << entry.pc;
		else	cout << setw(12) << "-";
		cout << setfill(' ') << setw(10);
		if (entry.pc==UNDEFINED) cout << "-";
		else cout << stage_names[entry.state];
		if (entry.destination==UNDEFINED) cout << setw(6) << "-";
		else{
			if (instruction.opcode == SW || instruction.opcode == SWS)
				cout << setw(6) << dec << entry.destination;
			else if (entry.destination < NUM_GP_REGISTERS)
				cout << setw(5) << "R" << dec << entry.destination;
			else
				cout << setw(5) << "F" << dec << entry.destination-NUM_GP_REGISTERS;
		}
		if (entry.value!=UNDEFINED) cout << "  0x" << hex << setw(8) << setfill('0') << entry.value << endl;
		else cout << setw(12) << setfill(' ') << "-" << endl;
	}
	cout << endl;
}

/* prints the content of the reservation stations */
void sim_ooo::print_reservation_stations(){
	cout << "RESERVATION STATIONS" << endl;
	cout  << setfill(' ');
	cout << setw(7) << "Name" << setw(6) << "Busy" << setw(12) << "PC" << setw(12) << "Vj" << setw(12) << "Vk" << setw(6) << "Qj" << setw(6) << "Qk" << setw(6) << "Dest" << setw(12) << "Address" << endl;
	for(unsigned i=0; i< reservation_stations.num_entries;i++){
		res_station_entry_t entry = reservation_stations.entries[i];
	 	cout  << setfill(' ');
		cout << setw(6);
		cout << res_station_names[entry.type];
		cout << entry.name + 1;
		cout << setw(6);
		if (entry.pc==UNDEFINED) cout << "no"; else cout << "yes";
		if (entry.pc!= UNDEFINED ) cout << setw(4) << "  0x" << hex << setfill('0') << setw(8) << entry.pc;
		else	cout << setfill(' ') << setw(12) <<  "-";
		if (entry.value1!= UNDEFINED ) cout << "  0x" << setfill('0') << setw(8) << hex << entry.value1;
		else	cout << setfill(' ') << setw(12) << "-";
		if (entry.value2!= UNDEFINED ) cout << "  0x" << setfill('0') << setw(8) << hex << entry.value2;
		else	cout << setfill(' ') << setw(12) << "-";
		cout << setfill(' ');
		cout <<setw(6);
		if (entry.tag1!= UNDEFINED ) cout << dec << entry.tag1;
		else	cout << "-";
		cout <<setw(6);
		if (entry.tag2!= UNDEFINED ) cout << dec << entry.tag2;
		else	cout << "-";
		cout <<setw(6);
		if (entry.destination!= UNDEFINED ) cout << dec << entry.destination;
		else	cout << "-";
		if (entry.address != UNDEFINED ) cout <<setw(4) << "  0x" << setfill('0') << setw(8) << hex << entry.address;
		else	cout << setfill(' ') << setw(12) <<  "-";
		cout << endl;
	}
	cout << endl;
}

/* prints the state of the pending instructions */
void sim_ooo::print_pending_instructions(){
	cout << "PENDING INSTRUCTIONS STATUS" << endl;
	cout << setfill(' ');
	cout << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7) << "WR" << setw(7) << "Commit";
	cout << endl;
	for(unsigned i=0; i< pending_instructions.num_entries;i++){
		instr_window_entry_t entry = pending_instructions.entries[i];
		if (entry.pc!= UNDEFINED ) cout << "0x" << setfill('0') << setw(8) << hex << entry.pc;
		else	cout << setfill(' ') << setw(10)  << "-";
		cout << setfill(' ');
		cout << setw(7);
		if (entry.issue!= UNDEFINED ) cout << dec << entry.issue;
		else	cout << "-";
		cout << setw(7);
		if (entry.exe!= UNDEFINED ) cout << dec << entry.exe;
		else	cout << "-";
		cout << setw(7);
		if (entry.wr!= UNDEFINED ) cout << dec << entry.wr;
		else	cout << "-";
		cout << setw(7);
		if (entry.commit!= UNDEFINED ) cout << dec << entry.commit;
		else	cout << "-";
		cout << endl;
	}
	cout << endl;
}


/* initializes the execution log */
void sim_ooo::init_log(){
	log << "EXECUTION LOG" << endl;
	log << setfill(' ');
	log << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7) << "WR" << setw(7) << "Commit";
	log << endl;
}

/* adds an instruction to the log */
void sim_ooo::commit_to_log(instr_window_entry_t entry){
    if (entry.pc!= UNDEFINED ) log << "0x" << setfill('0') << setw(8) << hex << entry.pc;
    else    log << setfill(' ') << setw(10)  << "-";
    log << setfill(' ');
    log << setw(7);
    if (entry.issue!= UNDEFINED ) log << dec << entry.issue;
    else    log << "-";
    log << setw(7);
    if (entry.exe!= UNDEFINED ) log << dec << entry.exe;
    else    log << "-";
    log << setw(7);
    if (entry.wr!= UNDEFINED ) log << dec << entry.wr;
    else    log << "-";
    log << setw(7);
    if (entry.commit!= UNDEFINED ) log << dec << entry.commit;
    else    log << "-";
    log << endl;
}

/* prints the content of the log */
void sim_ooo::print_log(){
	cout << log.str();
}

/* prints the state of the pending instruction, the content of the ROB, the content of the reservation stations and of the registers */
void sim_ooo::print_status(){
	print_pending_instructions();
	print_rob();
	print_reservation_stations();
	print_registers();
}

/* execution statistics */

float sim_ooo::get_IPC(){return (float)instructions_executed/clock_cycles;}

unsigned sim_ooo::get_instructions_executed(){return instructions_executed;}

unsigned sim_ooo::get_clock_cycles(){return clock_cycles;}



/* ============================================================================

   PARSER

   =========================================================================== */


void sim_ooo::load_program(const char *filename, unsigned base_address){

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
		case AND:
		case MULT:
		case DIV:
		case ADDS:
		case SUBS:
		case MULTS:
		case DIVS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "RF"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "RF"));
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
		case LWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
		case SWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "RF"));
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
    Program_Counter = instr_base_address;
}

/* ============================================================================

   Simulator creation, initialization and deallocation

   =========================================================================== */

sim_ooo::sim_ooo(unsigned mem_size,
    unsigned rob_size,
    unsigned num_int_res_stations,
    unsigned num_add_res_stations,
    unsigned num_mul_res_stations,
    unsigned num_load_res_stations,
    unsigned max_issue){
	//memory
	data_memory_size = mem_size;
	data_memory = new unsigned char[data_memory_size];

	//issue width
	issue_width = max_issue;

	//rob, instruction window, reservation stations
	rob.num_entries=rob_size;
	pending_instructions.num_entries=rob_size;
	reservation_stations.num_entries= num_int_res_stations+num_load_res_stations+num_add_res_stations+num_mul_res_stations;
	rob.entries = new rob_entry_t[rob_size];
	pending_instructions.entries = new instr_window_entry_t[rob_size];
	reservation_stations.entries = new res_station_entry_t[reservation_stations.num_entries];
	unsigned n=0;
	for (unsigned i=0; i<num_int_res_stations; i++,n++){
		reservation_stations.entries[n].type=INTEGER_RS;
		reservation_stations.entries[n].name=i;
	}
	for (unsigned i=0; i<num_load_res_stations; i++,n++){
		reservation_stations.entries[n].type=LOAD_B;
		reservation_stations.entries[n].name=i;
	}
	for (unsigned i=0; i<num_add_res_stations; i++,n++){
		reservation_stations.entries[n].type=ADD_RS;
		reservation_stations.entries[n].name=i;
	}
	for (unsigned i=0; i<num_mul_res_stations; i++,n++){
		reservation_stations.entries[n].type=MULT_RS;
		reservation_stations.entries[n].name=i;
	}
	//execution units
	num_units = 0;
	reset();
}

sim_ooo::~sim_ooo(){
	delete [] data_memory;
	delete [] rob.entries;
	delete [] pending_instructions.entries;
	delete [] reservation_stations.entries;
}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */

/* core of the simulator */
void sim_ooo::run(unsigned cycles){
    bool run2completion = (cycles == 0);
    instruction_t instr;
    opcode_t instr_op;
    opcode_t exec_op;
    opcode_t wr_op;
    opcode_t commit_op;
    unsigned IssueROB;
    unsigned IssueRES;
    unsigned ExecUnitIndex;
    unsigned ExecROB;
    unsigned WRRES;
    unsigned WRROB;
    unsigned WRoutput;
    unsigned CommitTag;
    unsigned CommitPC;
    unsigned CommitDest;
    unsigned CommitVal;

    while (cycles-- || run2completion){

        //Issue Stage
        for(unsigned i = 0; i <issue_width; i++) {
            instr = instr_memory[(Program_Counter - instr_base_address) >> 2]; //Divide PC by 4 to get instructions
            //cout << "Program Counter: " << hex << Program_Counter << endl;
            if (instr_op == EOP)    //if EOP if fetched, note the eop's program counter such that if the
                                    // last instruction is committed end RUN
                eop_pc = Program_Counter;

            instr_op = instr.opcode;
            IssueROB = get_free_ROB_entry();
            //if(IssueROB != UNDEFINED) cout << "Free ROB Found! Entry #" << IssueROB << endl;
            IssueRES = get_free_reservation_station(instr_op);
            //if(IssueRES != UNDEFINED) cout << "Free RES Found! Entry #" << IssueRES << endl;

            if (IssueRES != UNDEFINED && IssueROB != UNDEFINED) {
                set_reservation_station(instr, Program_Counter, IssueRES, IssueROB);
                set_ROB_entry(instr, Program_Counter, IssueROB);
                rob.entries[IssueROB].state = ISSUE;
                set_instr_window(IssueROB, ISSUE);
                Program_Counter = Program_Counter + 4;
            }
        }

        //Execution Stage
        for(unsigned i = 0; i < reservation_stations.num_entries; i++){
            exec_op = reservation_stations.entries[i].opcode;
            if(reservation_stations.entries[i].pc != UNDEFINED){
                if(reservation_stations.entries[i].ready && !reservation_stations.entries[i].executing){
                    ExecUnitIndex  = get_free_unit(exec_op);
                    if(ExecUnitIndex != UNDEFINED){ // exec unit found
                        exec_units[ExecUnitIndex].busy = exec_units[ExecUnitIndex].latency + 1;
                        exec_units[ExecUnitIndex].pc = reservation_stations.entries[i].pc;
                        exec_units[ExecUnitIndex].inuse = true;
                        reservation_stations.entries[i].executing = true;   // prevent executing the same instruction
                        ExecROB = find_entry(reservation_stations.entries[i].pc);
                        rob.entries[ExecROB].state = EXECUTE;
                        set_instr_window(ExecROB, EXECUTE);
                        if(exec_op == LW || exec_op == LWS) reservation_stations.entries[i].address =
                                reservation_stations.entries[i].address + reservation_stations.entries[i].value1;
                    }
                }
                else if(!reservation_stations.entries[i].ready){
                    if(reservation_stations.entries[i].tag1 == UNDEFINED
                    && reservation_stations.entries[i].tag2 == UNDEFINED)
                        reservation_stations.entries[i].ready = true;
                }
            }
        }

        // Decrements busy for instructions being executed
        for(unsigned i = 0; i < num_units; i++){
            if(exec_units[i].busy > 0) exec_units[i].busy--;
        }

        //Write Result Stage
        //Find finished instructions
        for(unsigned i = 0; i < num_units; i++){
            if(exec_units[i].busy == 0 && exec_units[i].inuse){
                WRRES = find_reservation_station_instr(exec_units[i].pc);
                WRROB = find_entry(exec_units[i].pc);
                wr_op = reservation_stations.entries[WRRES].opcode;
                //cout << "Opcode in WR: " << wr_op << endl;
                if(is_memory(wr_op)){
                    rob.entries[WRROB].state = WRITE_RESULT;
                    set_instr_window(WRROB,WRITE_RESULT);
                    WRoutput = char2unsigned(data_memory + reservation_stations.entries[WRRES].address);
                    //cout << "Output is: "<< hex << WRoutput << endl;
                    CDB(WRROB,WRoutput);
                    clean_res_station(&reservation_stations.entries[WRRES]);
                }
                else{
                    rob.entries[WRROB].state = WRITE_RESULT;
                    set_instr_window(WRROB,WRITE_RESULT);
                    WRoutput = alu(wr_op, reservation_stations.entries[WRRES].value1, reservation_stations.entries[WRRES].value2,
                                   reservation_stations.entries[WRRES].immediate, exec_units[i].pc);
                    //cout << "Output is: "<< hex << WRoutput << endl;
                    CDB(WRROB,WRoutput);
                    clean_res_station(&reservation_stations.entries[WRRES]);
                }

                // clear execution units
                exec_units[i].inuse = false;
                exec_units[i].pc = UNDEFINED;
            }
        }

        //Commit Stage
        //Find the lowest ROB entry with lowest PC to commit
        CommitPC = rob.entries[0].pc;   //if entry 0 PC is undefined it still works 0xffffffff > any non-undefined pc
        CommitTag = 0;
        for(unsigned i = 0; i < rob.num_entries; i++){
            if(rob.entries[i].pc < CommitPC){
                CommitPC = rob.entries[i].pc;
                CommitTag = i;
            }
        }
        //Check entries for ready
        if(rob.entries[CommitTag].ready){
            commit_op = rob.entries[CommitTag].opcode;
            CommitDest = rob.entries[CommitTag].destination;
            CommitVal = rob.entries[CommitTag].value;
            //cout << "Commit opcode: " << commit_op << " Commit Value " << CommitVal << " Commit Dest " << CommitDest <<endl;
            if(commit_op == LW || is_int(commit_op)){
                set_int_register(CommitDest,CommitVal);
                rob.entries[CommitTag].state = COMMIT;
                set_instr_window(CommitTag,COMMIT);
                commit_to_log(pending_instructions.entries[CommitTag]);
            }
            else if(is_branch(commit_op)){
                if(CommitVal != rob.entries[CommitTag].pc + 4){
                    rob.entries[CommitTag].state = COMMIT;
                    set_instr_window(CommitTag,COMMIT);
                    commit_to_log(pending_instructions.entries[CommitTag]);
                    //cout << "Branch Mispredicted" << endl;
                    branchisfalse = true;
                }
                else{
                    rob.entries[CommitTag].state = COMMIT;
                    set_instr_window(CommitTag,COMMIT);
                    commit_to_log(pending_instructions.entries[CommitTag]);
                    //cout << "Branch is Correct" << endl;
                    branchisfalse = false;
                }
            }
            else if(commit_op == LWS || is_fp_alu(commit_op)){
                set_fp_register(CommitDest,unsigned2float(CommitVal));
                rob.entries[CommitTag].state = COMMIT;
                set_instr_window(CommitTag,COMMIT);
                commit_to_log(pending_instructions.entries[CommitTag]);
            }
            //cout << "Instruction: " << CommitPC << "is committed" << endl;
            if((rob.entries[CommitTag].pc == eop_pc - 4) && (!branchisfalse)) {
                eopend = true;
            }
            clear_entry(CommitTag);
            instructions_executed ++;
            if(branchisfalse){
                log_mispredict_instr(Program_Counter, CommitPC, CommitVal);
                mispredict();
                Program_Counter = CommitVal;
                branchisfalse = false;
            }
            clean_instr_window(&pending_instructions.entries[CommitTag]);
        }
         //check for instruction that is ready to be committed
        for (unsigned i = 0; i < rob.num_entries; i++) {
            if (rob.entries[i].value != UNDEFINED && rob.entries[i].pc != UNDEFINED) {
                rob.entries[i].ready = true;
            }
        }

        clock_cycles++;
        if(eopend) return;
    }
}

//reset the state of the simulator - please complete
void sim_ooo::reset(){

	//init instruction log
	init_log();

	// data memory
	for (unsigned i=0; i<data_memory_size; i++) data_memory[i]=0xFF;

	//instr memory
	for (unsigned i=0; i<PROGRAM_SIZE;i++){
		instr_memory[i].opcode=(opcode_t)EOP;
		instr_memory[i].src1=UNDEFINED;
		instr_memory[i].src2=UNDEFINED;
		instr_memory[i].dest=UNDEFINED;
		instr_memory[i].immediate=UNDEFINED;
	}

	//general purpose registers
    for(unsigned i = 0; i < NUM_GP_REGISTERS; i++){
        set_int_register(i,UNDEFINED);
        set_fp_register(i,unsigned2float(UNDEFINED)) ;
    }
	//pending_instructions
    //reset_instr_window();

	//rob
    //clear_all_ROB();

	//reservation_stations
    //clear_all_reservation_station();

    //execution units
    //clear_exec_units();

    //register renaming tags
    //clear_regtag();

	mispredict();

	//execution statistics
	clock_cycles = 0;
	instructions_executed = 0;

	//other required initializations
}

/* registers related */

int sim_ooo::get_int_register(unsigned reg){
	return int_registers[reg]; //please modify
}

void sim_ooo::set_int_register(unsigned reg, int value){
    int_registers[reg] = value;
}

float sim_ooo::get_fp_register(unsigned reg){
	return unsigned2float(fp_registers[reg]); //please modify
}

void sim_ooo::set_fp_register(unsigned reg, float value){
    if(reg >= 32) reg = reg - 32;
    fp_registers[reg] = float2unsigned(value);
}

unsigned sim_ooo::get_int_register_tag(unsigned reg){
    for(unsigned i = 0; i < rob.num_entries; i++){
        if(rob.entries[i].destination == reg) {
            for(unsigned j = i + 1; j < rob.num_entries; j++){
                if(rob.entries[j].destination == reg && rob.entries[j].pc > rob.entries[i].pc) return j;
            }
            return i;
        }
    }
	return UNDEFINED; //please modify
}

unsigned sim_ooo::get_fp_register_tag(unsigned reg){
    for(unsigned i = 0; i < rob.num_entries; i++){
        if(rob.entries[i].destination == (reg + NUM_GP_REGISTERS)){
            for(unsigned j = i + 1; j < rob.num_entries; j++){
                if(rob.entries[j].destination == (reg + NUM_GP_REGISTERS) && rob.entries[j].pc > rob.entries[i].pc) return j;
            }
            return i;
        }
    }
	return UNDEFINED; //please modify
}

unsigned sim_ooo::get_regtag(unsigned index) {
    return regtag[index].tag;
}

void sim_ooo::clear_all_ROB() {
    for(unsigned i = 0; i <rob.num_entries; i++){
        clean_rob(&rob.entries[i]);
    }
    //location tracker reset
    last_entry = 0;
}

void sim_ooo::clear_exec_units() {
    for(unsigned i = 0; i < num_units; i++){
        exec_units[i].busy = 0;
        exec_units[i].pc = UNDEFINED;
        exec_units[i].inuse = false;
    }
}

void sim_ooo::clear_all_reservation_station() {
    for(unsigned i = 0; i < reservation_stations.num_entries; i++){
        clean_res_station(&reservation_stations.entries[i]);
    }
}

void sim_ooo::clear_regtag() {
    for(unsigned i = 0; i <NUM_GP_REGISTERS; i++){
        regtag[i].tag = UNDEFINED;
        regtag[i].pc = UNDEFINED;
        regtag[i + NUM_GP_REGISTERS].tag = UNDEFINED;
        regtag[i + NUM_GP_REGISTERS].pc = UNDEFINED;
    }
}

void sim_ooo::reset_instr_window() {
    for(unsigned i = 0; i <rob.num_entries; i++) {
        clean_instr_window(&pending_instructions.entries[i]);
    }
}

void sim_ooo::set_instr_window(unsigned int rob_index, stage_t instr_stage) {
    switch (instr_stage) {
        case ISSUE:
            pending_instructions.entries[rob_index].pc = rob.entries[rob_index].pc;
            pending_instructions.entries[rob_index].issue = clock_cycles;
            break;
        case EXECUTE:
            pending_instructions.entries[rob_index].exe = clock_cycles;
            break;
        case WRITE_RESULT:
            pending_instructions.entries[rob_index].wr = clock_cycles;
            break;
        case COMMIT:
            pending_instructions.entries[rob_index].commit = clock_cycles;
            break;
        default:
            break;

    }
}

unsigned sim_ooo::get_free_ROB_entry() {
    //ring buffer like
    //locate last rob buffer used starting at 0
   for(unsigned i = last_entry; i < rob.num_entries; i++){
       if(rob.entries[i].pc == UNDEFINED){
           last_entry = i;
           return i;
       }
   }
    // If the ROB is full from last_entry to  rob.num_entries - 1 and search again from the beginning
    //if(last_entry == (rob.num_entries - 1)){
    for(unsigned i = 0; i < last_entry; i++){
        if(rob.entries[i].pc == UNDEFINED){
            last_entry = i;
            return i;
        }
    }
    return UNDEFINED;
}

void sim_ooo::set_ROB_entry(instruction_t instruction, unsigned int PC, unsigned int ROB_index) {
    rob.entries[ROB_index].ready = false;
    rob.entries[ROB_index].pc = PC;
    rob.entries[ROB_index].state = ISSUE;
    rob.entries[ROB_index].opcode = instruction.opcode;

    if(instruction.opcode == LW || is_int(instruction.opcode)){
        rob.entries[ROB_index].destination = instruction.dest;
        regtag[instruction.dest].tag = ROB_index;
        regtag[instruction.dest].op = instruction.opcode;
        regtag[instruction.dest].pc = PC;
    }
    else if(instruction.opcode == LWS || is_fp_alu(instruction.opcode)){
        rob.entries[ROB_index].destination = instruction.dest + NUM_GP_REGISTERS;
        regtag[instruction.dest + NUM_GP_REGISTERS].tag = ROB_index;
        regtag[instruction.dest + NUM_GP_REGISTERS].op = instruction.opcode;
        regtag[instruction.dest + NUM_GP_REGISTERS].pc = PC;
    }
}

unsigned sim_ooo::find_entry(unsigned instr_pc) {
    for(unsigned i = 0; i < rob.num_entries; i++){
        if(rob.entries[i].pc == instr_pc) return i;
    }
    return UNDEFINED;
}

void sim_ooo::clear_entry(unsigned int rename_tag) {
    if(!is_branch(rob.entries[rename_tag].opcode)) {
        /*if (rob.entries[rename_tag].opcode == regtag[rob.entries[rename_tag].destination].op) {
            regtag[rob.entries[rename_tag].destination].tag = UNDEFINED;
        }*/
        if (rob.entries[rename_tag].pc == regtag[rob.entries[rename_tag].destination].pc) {
            regtag[rob.entries[rename_tag].destination].tag = UNDEFINED;
            regtag[rob.entries[rename_tag].destination].pc = UNDEFINED;
        }
    }
    clean_rob(&rob.entries[rename_tag]);
    //cout << "Rob PC: " << rob.entries[rename_tag].pc << " Rob dest: " << rob.entries[rename_tag].destination
    //        << " Rob op: " << rob.entries[rename_tag].opcode << " Rob value: " << rob.entries[rename_tag].value
    //       << " Rob ready: " << rob.entries[rename_tag].ready << endl;
}

unsigned sim_ooo::get_free_reservation_station(opcode_t opcode) {
    if(is_memory(opcode)){
        for(unsigned i = 0; i < reservation_stations.num_entries; i++){
            if(reservation_stations.entries[i].type == LOAD_B
               && reservation_stations.entries[i].pc == UNDEFINED)
                return i;
        }
    }
    else if(is_int(opcode) || is_branch(opcode)){
        for(unsigned i = 0; i < reservation_stations.num_entries; i++){
            if(reservation_stations.entries[i].type == INTEGER_RS
               && reservation_stations.entries[i].pc == UNDEFINED)
                return i;
        }
    }
    else{
        switch (opcode) {
            //FP Adder
            case ADDS:
            case SUBS:
                for(unsigned i = 0; i < reservation_stations.num_entries; i++){
                    if(reservation_stations.entries[i].type == ADD_RS
                       && reservation_stations.entries[i].pc == UNDEFINED)
                        return i;
                }
                break;
            // Mults and Divs
            case MULT:
            case MULTS:
            case DIV:
            case DIVS:
                for(unsigned i = 0; i < reservation_stations.num_entries; i++){
                    if(reservation_stations.entries[i].type == MULT_RS
                       && reservation_stations.entries[i].pc == UNDEFINED)
                        return i;
                }
                break;
            default:
                break;
        }
    }
    return UNDEFINED;
}

void sim_ooo::set_reservation_station
(instruction_t instruction, unsigned int PC,
 unsigned int res_index, unsigned int ROB_index) {
    //unsigned Vj = get_int_register(instruction.src1);
    //unsigned Vk = get_int_register(instruction.src2);
    //unsigned Qj = get_regtag(instruction.src1);
    //unsigned Qk = get_regtag(instruction.src2);
    unsigned j; // Vj/Vk is unpredictable
    unsigned k; //

    reservation_stations.entries[res_index].pc = PC;
    reservation_stations.entries[res_index].opcode = instruction.opcode;
    reservation_stations.entries[res_index].destination = ROB_index;
    if(is_memory(instruction.opcode)){
        j = get_regtag(instruction.src1);
        if(j != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[j].ready) {
                reservation_stations.entries[res_index].value1 = UNDEFINED;
                reservation_stations.entries[res_index].tag1 = j;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value1 = rob.entries[j].value;
                reservation_stations.entries[res_index].tag1 = UNDEFINED;   // ROB has temp vals
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value1 = get_int_register(instruction.src1);
            reservation_stations.entries[res_index].tag1 = UNDEFINED;   //no TAG
        }
        reservation_stations.entries[res_index].value2 = UNDEFINED;
        reservation_stations.entries[res_index].tag2 = UNDEFINED;
        reservation_stations.entries[res_index].address = instruction.immediate;
    }
    else if(is_int_r(instruction.opcode)){
        j = get_regtag(instruction.src1);
        k = get_regtag(instruction.src2);
        if(j != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[j].ready) {
                reservation_stations.entries[res_index].value1 = UNDEFINED;
                reservation_stations.entries[res_index].tag1 = j;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value1 = rob.entries[j].value;
                reservation_stations.entries[res_index].tag1 = UNDEFINED;   // ROB has temp vals
            }
            if(rob.entries[j].pc == PC) { // instruction destination is same as src1
                reservation_stations.entries[res_index].value1 = get_int_register(instruction.src1);
                reservation_stations.entries[res_index].tag1 = UNDEFINED;
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value1 = get_int_register(instruction.src1);
            reservation_stations.entries[res_index].tag1 = UNDEFINED;   //no TAG
        }
        if(k != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[k].ready) {
                reservation_stations.entries[res_index].value2 = UNDEFINED;
                reservation_stations.entries[res_index].tag2 = k;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value2 = rob.entries[k].value;
                reservation_stations.entries[res_index].tag2 = UNDEFINED;   // ROB has temp vals
            }
            if(rob.entries[k].pc == PC) { // instruction destination is same as src2
                reservation_stations.entries[res_index].value2 = get_int_register(instruction.src2);
                reservation_stations.entries[res_index].tag2 = UNDEFINED;
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value2 = get_int_register(instruction.src2);
            reservation_stations.entries[res_index].tag2 = UNDEFINED;   //no TAG
        }
    }
    else if(is_int_imm(instruction.opcode)){
        j = get_regtag(instruction.src1);
        if(j != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[j].ready) {
                reservation_stations.entries[res_index].value1 = UNDEFINED;
                reservation_stations.entries[res_index].tag1 = j;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value1 = rob.entries[j].value;
                reservation_stations.entries[res_index].tag1 = UNDEFINED;   // ROB has temp vals
            }
            if(rob.entries[j].pc == PC) { // instruction destination is same as src1
                reservation_stations.entries[res_index].value1 = get_int_register(instruction.src1);
                reservation_stations.entries[res_index].tag1 = UNDEFINED;
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value1 = get_int_register(instruction.src1);
            reservation_stations.entries[res_index].tag1 = UNDEFINED;   //no TAG
        }
        reservation_stations.entries[res_index].tag2 = UNDEFINED;
        reservation_stations.entries[res_index].immediate = instruction.immediate;
    }
    else if(is_branch(instruction.opcode)){
        j = get_regtag(instruction.src1);
        if(j != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[j].ready) {
                reservation_stations.entries[res_index].value1 = UNDEFINED;
                reservation_stations.entries[res_index].tag1 = j;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value1 = rob.entries[j].value;
                reservation_stations.entries[res_index].tag1 = UNDEFINED;   // ROB has temp vals
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value1 = get_int_register(instruction.src1);
            reservation_stations.entries[res_index].tag1 = UNDEFINED;   //no TAG
        }
        reservation_stations.entries[res_index].tag2 = UNDEFINED;
        reservation_stations.entries[res_index].immediate = instruction.immediate;
    }
    else if(is_fp_alu(instruction.opcode)){
        j = get_regtag(instruction.src1 + NUM_GP_REGISTERS);
        k = get_regtag(instruction.src2 + NUM_GP_REGISTERS);
        if(j != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[j].ready) {
                reservation_stations.entries[res_index].value1 = UNDEFINED;
                reservation_stations.entries[res_index].tag1 = j;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value1 = rob.entries[j].value;
                reservation_stations.entries[res_index].tag1 = UNDEFINED;   // ROB has temp vals
            }
            if(rob.entries[j].pc == PC) { // instruction destination is same as src1
                reservation_stations.entries[res_index].value1 = float2unsigned(get_fp_register(instruction.src1));
                reservation_stations.entries[res_index].tag1 = UNDEFINED;
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value1 = float2unsigned(get_fp_register(instruction.src1));
            reservation_stations.entries[res_index].tag1 = UNDEFINED;   //no TAG
        }
        if(k != UNDEFINED){ // check for RAW hazard
            if(!rob.entries[k].ready) {
                reservation_stations.entries[res_index].value2 = UNDEFINED;
                reservation_stations.entries[res_index].tag2 = k;   // waits for ready in ROB
            }
            else{
                reservation_stations.entries[res_index].value2 = rob.entries[k].value;
                reservation_stations.entries[res_index].tag2 = UNDEFINED;   // ROB has temp vals
            }
            if(rob.entries[k].pc == PC) { // instruction destination is same as src2
                reservation_stations.entries[res_index].value2 = float2unsigned(get_fp_register(instruction.src2));
                reservation_stations.entries[res_index].tag2 = UNDEFINED;
            }
        }
        else {   // no renaming, no RAW
            reservation_stations.entries[res_index].value2 = float2unsigned(get_fp_register(instruction.src2));
            reservation_stations.entries[res_index].tag2 = UNDEFINED;   //no TAG
        }
    }
}

unsigned sim_ooo::find_reservation_station_instr(unsigned instr_pc) {
    for(unsigned i = 0; i < reservation_stations.num_entries; i++){
        if(instr_pc == reservation_stations.entries[i].pc) return i;
    }
    return UNDEFINED;
}

void sim_ooo::CDB(unsigned rename_tag, unsigned output) {
    //add output into reservation station and rob
    //cout << " CDB output value: " << output << endl;
    for(unsigned i = 0; i <reservation_stations.num_entries; i++){
        if(reservation_stations.entries[i].tag1 == rename_tag){
            reservation_stations.entries[i].value1 = output;
            //cout << " Tag 1 output value: " << reservation_stations.entries[i].value1 << endl;
            reservation_stations.entries[i].tag1 = UNDEFINED;
            if(!reservation_stations.entries[i].ready){
                if(reservation_stations.entries[i].tag1 == UNDEFINED
                   && reservation_stations.entries[i].tag2 == UNDEFINED)
                    reservation_stations.entries[i].ready = true;
            }
        }
        if(reservation_stations.entries[i].tag2 == rename_tag){
            reservation_stations.entries[i].value2 = output;
            //cout << " Tag 2 output value: " << reservation_stations.entries[i].value2 << endl;
            reservation_stations.entries[i].tag2 = UNDEFINED;
            if(!reservation_stations.entries[i].ready){
                if(reservation_stations.entries[i].tag1 == UNDEFINED
                   && reservation_stations.entries[i].tag2 == UNDEFINED)
                    reservation_stations.entries[i].ready = true;
            }
        }
    }
    rob.entries[rename_tag].value = output;
}

void sim_ooo::log_mispredict_instr(unsigned int pc, unsigned int branchpc, unsigned int branchtarget) {
    unsigned branch2target;
    unsigned branch2pc;
    unsigned instr_after_branch = branchpc + 4;
    //cout << "branch pc: " << branchpc << " target: " << branchtarget << " Program Counter: " << pc << endl;

    if(pc > branchtarget) {
        branch2pc = (pc - branchpc) >> 2; //divide by 4
        for (unsigned i = 0; i < branch2pc; i++) { //find the instruction in pending via it's pc
            for (unsigned j = 0; j < rob.num_entries; j++) {
                //cout << "#" << j << " pending pc: " << pending_instructions.entries[j].pc << " check pc: " << (instr_after_branch + (4*i))<< endl;
                if (pending_instructions.entries[j].pc == instr_after_branch + (4 * i)) {
                    commit_to_log(pending_instructions.entries[j]);
                }
            }
        }
    }
    else{
        branch2target = (branchtarget - branchpc) >> 2; //divide by 4
        for(unsigned i = 0; i < branch2target; i++){ //find the instruction in pending via it's pc
            for(unsigned j = 0; j < rob.num_entries; j++){
                //cout << "#" << j << " pending pc: " << pending_instructions.entries[j].pc << " check pc: " << (instr_after_branch + (4*i))<< endl;
                if(pending_instructions.entries[j].pc == instr_after_branch + (4*i)){
                    commit_to_log(pending_instructions.entries[j]);
                }
            }
        }
    }
}

void sim_ooo::mispredict() {
    reset_instr_window();
    clear_all_ROB();
    clear_all_reservation_station();
    clear_exec_units();
    clear_regtag();
}