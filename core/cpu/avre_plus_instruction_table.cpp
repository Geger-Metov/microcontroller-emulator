#include "cpu/avr_instructions.h"
#include "decoder/avr_decode_helpers.h"

// The ATmega328P (AVRe+) instruction set. The first match (mask/pattern)
// wins, but entries do not overlap — the order does not matter.
// cycles is the base cost; cycles for a taken branch / skip are added
// in exec_* through add_cycles().
using namespace avr;

const avr::InstructionDesc avr::avre_plus_instruction_table[]{
    // --- 0000 ---
    {0xFFFF, 0x0000, 1, 1, decode_none, exec_NOP},
    {0xFF00, 0x0100, 1, 1, decode_Rd_Rr_pair, exec_MOVW},
    {0xFF00, 0x0200, 1, 2, decode_Rd16_Rr16, exec_MULS},
    {0xFF88, 0x0300, 1, 2, decode_Rd16_Rr16_3bit, exec_MULSU},
    {0xFF88, 0x0308, 1, 2, decode_Rd16_Rr16_3bit, exec_FMUL},
    {0xFF88, 0x0380, 1, 2, decode_Rd16_Rr16_3bit, exec_FMULS},
    {0xFF88, 0x0388, 1, 2, decode_Rd16_Rr16_3bit, exec_FMULSU},
    {0xFC00, 0x0400, 1, 1, decode_Rd_Rr, exec_CPC},
    {0xFC00, 0x0800, 1, 1, decode_Rd_Rr, exec_SBC},
    {0xFC00, 0x0C00, 1, 1, decode_Rd_Rr, exec_ADD}, // LSL Rd = ADD Rd,Rd
    // --- 0001 / 0010 ---
    {0xFC00, 0x1000, 1, 1, decode_Rd_Rr, exec_CPSE},
    {0xFC00, 0x1400, 1, 1, decode_Rd_Rr, exec_CP},
    {0xFC00, 0x1800, 1, 1, decode_Rd_Rr, exec_SUB},
    {0xFC00, 0x1C00, 1, 1, decode_Rd_Rr, exec_ADC}, // ROL Rd = ADC Rd,Rd
    {0xFC00, 0x2000, 1, 1, decode_Rd_Rr, exec_AND}, // TST Rd = AND Rd,Rd
    {0xFC00, 0x2400, 1, 1, decode_Rd_Rr, exec_EOR}, // CLR Rd = EOR Rd,Rd
    {0xFC00, 0x2800, 1, 1, decode_Rd_Rr, exec_OR},
    {0xFC00, 0x2C00, 1, 1, decode_Rd_Rr, exec_MOV},
    // --- 0011..0111: operations with a constant ---
    {0xF000, 0x3000, 1, 1, decode_Rd_imm, exec_CPI},
    {0xF000, 0x4000, 1, 1, decode_Rd_imm, exec_SBCI},
    {0xF000, 0x5000, 1, 1, decode_Rd_imm, exec_SUBI},
    {0xF000, 0x6000, 1, 1, decode_Rd_imm, exec_ORI},  // SBR
    {0xF000, 0x7000, 1, 1, decode_Rd_imm, exec_ANDI}, // CBR
    // --- 10q0 qq0d dddd yqqq: LDD/STD (LD/ST Y/Z without displacement — q=0)
    // ---
    {0xD208, 0x8000, 1, 2, decode_ldd_std, exec_LD}, // Z
    {0xD208, 0x8008, 1, 2, decode_ldd_std, exec_LD}, // Y
    {0xD208, 0x8200, 1, 2, decode_ldd_std, exec_ST}, // Z
    {0xD208, 0x8208, 1, 2, decode_ldd_std, exec_ST}, // Y
    // --- 1001 000d dddd xxxx: loads ---
    {0xFE0F, 0x9000, 2, 2, decode_Rd_k16, exec_LDS},
    {0xFE0F, 0x9001, 1, 2, decode_ld_st_ptr, exec_LD}, // Z+
    {0xFE0F, 0x9002, 1, 2, decode_ld_st_ptr, exec_LD}, // -Z
    {0xFE0F, 0x9004, 1, 3, decode_lpm_rd_z, exec_LPM}, // LPM Rd, Z
    {0xFE0F, 0x9005, 1, 3, decode_lpm_rd_z, exec_LPM}, // LPM Rd, Z+
    {0xFE0F, 0x9009, 1, 2, decode_ld_st_ptr, exec_LD}, // Y+
    {0xFE0F, 0x900A, 1, 2, decode_ld_st_ptr, exec_LD}, // -Y
    {0xFE0F, 0x900C, 1, 2, decode_ld_st_ptr, exec_LD}, // X
    {0xFE0F, 0x900D, 1, 2, decode_ld_st_ptr, exec_LD}, // X+
    {0xFE0F, 0x900E, 1, 2, decode_ld_st_ptr, exec_LD}, // -X
    {0xFE0F, 0x900F, 1, 2, decode_Rd, exec_POP},
    // --- 1001 001d dddd xxxx: stores ---
    {0xFE0F, 0x9200, 2, 2, decode_Rd_k16, exec_STS},
    {0xFE0F, 0x9201, 1, 2, decode_ld_st_ptr, exec_ST}, // Z+
    {0xFE0F, 0x9202, 1, 2, decode_ld_st_ptr, exec_ST}, // -Z
    {0xFE0F, 0x9209, 1, 2, decode_ld_st_ptr, exec_ST}, // Y+
    {0xFE0F, 0x920A, 1, 2, decode_ld_st_ptr, exec_ST}, // -Y
    {0xFE0F, 0x920C, 1, 2, decode_ld_st_ptr, exec_ST}, // X
    {0xFE0F, 0x920D, 1, 2, decode_ld_st_ptr, exec_ST}, // X+
    {0xFE0F, 0x920E, 1, 2, decode_ld_st_ptr, exec_ST}, // -X
    {0xFE0F, 0x920F, 1, 2, decode_Rd, exec_PUSH},
    // --- 1001 010d dddd xxxx: single-operand ---
    {0xFE0F, 0x9400, 1, 1, decode_Rd, exec_COM},
    {0xFE0F, 0x9401, 1, 1, decode_Rd, exec_NEG},
    {0xFE0F, 0x9402, 1, 1, decode_Rd, exec_SWAP},
    {0xFE0F, 0x9403, 1, 1, decode_Rd, exec_INC},
    {0xFE0F, 0x9405, 1, 1, decode_Rd, exec_ASR},
    {0xFE0F, 0x9406, 1, 1, decode_Rd, exec_LSR},
    {0xFE0F, 0x9407, 1, 1, decode_Rd, exec_ROR},
    {0xFE0F, 0x940A, 1, 1, decode_Rd, exec_DEC},
    // --- BSET/BCLR (SEC..SEI / CLC..CLI) ---
    {0xFF8F, 0x9408, 1, 1, decode_sreg_bit, exec_BSET},
    {0xFF8F, 0x9488, 1, 1, decode_sreg_bit, exec_BCLR},
    // --- control ---
    {0xFFFF, 0x9409, 1, 2, decode_none, exec_IJMP},
    {0xFFFF, 0x9508, 1, 4, decode_none, exec_RET},
    {0xFFFF, 0x9509, 1, 3, decode_none, exec_ICALL},
    {0xFFFF, 0x9518, 1, 4, decode_none, exec_RETI},
    {0xFFFF, 0x9588, 1, 1, decode_none, exec_SLEEP},
    {0xFFFF, 0x9598, 1, 1, decode_none, exec_NOP}, // BREAK
    {0xFFFF, 0x95A8, 1, 1, decode_none,
     exec_NOP}, // WDR (the WDT is not modeled)
    {0xFFFF, 0x95C8, 1, 3, decode_none, exec_LPM_R0},
    {0xFFFF, 0x95E8, 1, 1, decode_none, exec_SPM},
    {0xFE0E, 0x940C, 2, 3, decode_abs22, exec_JMP},
    {0xFE0E, 0x940E, 2, 4, decode_abs22, exec_CALL},
    // --- words ---
    {0xFF00, 0x9600, 1, 2, decode_Rd_pair_K, exec_ADIW},
    {0xFF00, 0x9700, 1, 2, decode_Rd_pair_K, exec_SBIW},
    // --- I/O bits ---
    {0xFF00, 0x9800, 1, 2, decode_io_bit, exec_CBI},
    {0xFF00, 0x9900, 1, 1, decode_io_bit, exec_SBIC},
    {0xFF00, 0x9A00, 1, 2, decode_io_bit, exec_SBI},
    {0xFF00, 0x9B00, 1, 1, decode_io_bit, exec_SBIS},
    // --- MUL ---
    {0xFC00, 0x9C00, 1, 2, decode_Rd_Rr, exec_MUL},
    // --- I/O, jumps, LDI ---
    {0xF800, 0xB000, 1, 1, decode_Rd_IO, exec_IN},
    {0xF800, 0xB800, 1, 1, decode_IO_Rr, exec_OUT},
    {0xF000, 0xC000, 1, 2, decode_rel12, exec_RJMP},
    {0xF000, 0xD000, 1, 3, decode_rel12, exec_RCALL},
    {0xF000, 0xE000, 1, 1, decode_Rd_imm, exec_LDI}, // SER = LDI Rd,0xFF
    // --- conditional branches and register bits ---
    {0xFC00, 0xF000, 1, 1, decode_rel7_s3, exec_BRBS},
    {0xFC00, 0xF400, 1, 1, decode_rel7_s3, exec_BRBC},
    {0xFE08, 0xF800, 1, 1, decode_Rd_bit, exec_BLD},
    {0xFE08, 0xFA00, 1, 1, decode_Rd_bit, exec_BST},
    {0xFE08, 0xFC00, 1, 1, decode_Rd_bit, exec_SBRC},
    {0xFE08, 0xFE00, 1, 1, decode_Rd_bit, exec_SBRS},
};

const std::size_t avr::avre_plus_instruction_table_size =
    sizeof(avre_plus_instruction_table) /
    sizeof(avre_plus_instruction_table[0]);
