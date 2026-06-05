#include "../decoder/avr_decode_helpers.h"
#include "avr_instructions.h"

const avr::InstructionDesc avr::avre_plus_instruction_table[]{
    // NOP (0000 0000 0000 0000)
    {.mask = 0xFFFF,
     .pattern = 0x0000,
     .length = 1,
     .cycles = 1,
     .decode = avr::decode_none,
     .execute = avr::exec_NOP},
    // RET (1001 0101 0000 1000)
    {0xFFFF, 0x9508, 1, 4, avr::decode_none, avr::exec_RET},
    // RETI (1001 0101 0001 1000)
    {0xFFFF, 0X9518, 1, 4, avr::decode_none, avr::exec_RETI},
    // MOV (0010 11rd dddd rrrr)
    {0xFC00, 0x2C00, 1, 1, avr::decode_Rd_Rr, avr::exec_MOV},
    // ADD (0000 11rd dddd rrrr)
    {0xFC00, 0x0C00, 1, 1, avr::decode_Rd_Rr, avr::exec_ADD},
    // RJMP (1100 kkkk kkkk kkkk)
    {0xF000, 0xC000, 1, 2, avr::decode_rel12, avr::exec_RJMP},
    // JMP
    {0xFE0E, 0x940C, 2, 3, avr::decode_abs22, avr::exec_JMP},
    // LDI (1110 KKKK dddd KKKK)
    {0xF000, 0xE000, 1, 1, avr::decode_Rd_imm, avr::exec_LDI},
    // OUT (1011 1AAr rrrr AAAA)
    {0xF800, 0xB800, 1, 1, avr::decode_IO_Rr, avr::exec_OUT},
    // IN (1011 0AAd dddd AAAA)
    {0xF800, 0xB000, 1, 1, avr::decode_Rd_IO, avr::exec_IN},
    // PUSH (1001 001d dddd 1111)
    {0xFE0F, 0x920F, 1, 2, avr::decode_Rd, avr::exec_PUSH},
    // POP (1001 000d dddd 1111)
    {0xFE0F, 0x900F, 1, 2, avr::decode_Rd, avr::exec_POP},
    // CP (0001 01rd dddd rrrr)
    {0xFC00, 0x1400, 1, 1, avr::decode_Rd_Rr, avr::exec_CP},
    // CPI (0011 KKKK dddd KKKK)
    {0xF000, 0x3000, 1, 1, avr::decode_Rd_imm, avr::exec_CPI},
    // SUBI (0101 KKKK dddd KKKK)
    {0xF000, 0x5000, 1, 1, avr::decode_Rd_imm, avr::exec_SUBI},
    // SBCI (0100 KKKK dddd KKKK)
    {0xF000, 0x4000, 1, 1, avr::decode_Rd_imm, avr::exec_SBCI},
    // ANDI (0111 KKKK dddd KKKK)
    {0xF000, 0x7000, 1, 1, avr::decode_Rd_imm, avr::exec_ANDI},
    // ORI  (0110 KKKK dddd KKKK)
    {0xF000, 0x6000, 1, 1, avr::decode_Rd_imm, avr::exec_ORI},
    // BRNE (1111 01kk kkkk k001)
    {0xFC07, 0xF401, 1, 1, avr::decode_rel7, avr::exec_BRNE},
    // BREQ (1111 00kk kkkk k001)
    {0xFC07, 0xF001, 1, 1, avr::decode_rel7, avr::exec_BREQ},
    // SBI (1001 1010 AAAA Abbb)
    {0xFF00, 0x9A00, 1, 2, avr::decode_io_bit, avr::exec_SBI},
    // CBI (1001 1000 AAAA Abbb)
    {0xFF00, 0x9800, 1, 2, avr::decode_io_bit, avr::exec_CBI},
    // RCALL (1101 kkkk kkkk kkkk)
    {0xF000, 0xD000, 1, 3, avr::decode_rel12, avr::exec_RCALL},
    // CALL
    {0xFE0E, 0x940E, 2, 4, avr::decode_abs22, avr::exec_CALL},
    // INC (1001 010d dddd 0011)
    {0xFE0F, 0x9403, 1, 1, avr::decode_Rd, avr::exec_INC},
    // DEC (1001 010d dddd 1010)
    {0xFE0F, 0x940A, 1, 1, avr::decode_Rd, avr::exec_DEC},
    // AND (0010 00rd dddd rrrr)
    {0xFC00, 0x2000, 1, 1, avr::decode_Rd_Rr, avr::exec_AND},
    // EOR (0010 01rd dddd rrrr)
    {0xFC00, 0x2400, 1, 1, avr::decode_Rd_Rr, avr::exec_EOR},
    // OR (0010 10rd dddd rrrr)
    {0xFC00, 0x2800, 1, 1, avr::decode_Rd_Rr, avr::exec_OR},
    // ADC (0001 11rd dddd rrrr)
    {0xFC00, 0x1C00, 1, 1, avr::decode_Rd_Rr, avr::exec_ADC},
    // SUB (0001 10rd dddd rrrr)
    {0xFC00, 0x1800, 1, 1, avr::decode_Rd_Rr, avr::exec_SUB},
    // SBC (0000 10rd dddd rrrr)
    {0xFC00, 0x0800, 1, 1, avr::decode_Rd_Rr, avr::exec_SBC},
    // BRCC (1111 01kk kkkk k000)
    // в exec добавить один цикл если условие верное
    {0xFC07, 0xF400, 1, 1, avr::decode_rel7, avr::exec_BRCC},
    // BRBC (1111 01kk kkkk ksss)
    // в exec добавить один цикл если условие верное
    {0xFC00, 0xF400, 1, 1, avr::decode_rel7_s3, avr::exec_BRBC},
    // CPSE (0001 00rd dddd rrrr)
    // в exec добавить один цикл если условие верное и пропущено одно слово
    // в exec добавить два цикла если условие верное и пропущено два слова
    {0xFC00, 0x1000, 1, 1, avr::decode_Rd_Rr, avr::exec_CPSE},
    // SBIC (1001 1001 AAAA Abbb)
    // в exec добавить один цикл если условие верное и пропущено одно слово
    // в exec добавить два цикла если условие верное и пропущено два слова
    {0xFF00, 0x9900, 1, 1, avr::decode_io_bit, avr::exec_SBIC},
    // SBIS (1001 1011 AAAA Abbb)
    // в exec добавить один цикл если условие верное и пропущено одно слово
    // в exec добавить два цикла если условие верное и пропущено два слова
    {0xFF00, 0x9B00, 1, 1, avr::decode_io_bit, avr::exec_SBIS},
    // ADIW (1001 0110 KKdd KKKK)
    {0xFF00, 0x9600, 1, 2, avr::decode_Rd_pair_K, avr::exec_ADIW},
    // SBIW (1001 0111 KKdd KKKK)
    {0xFF00, 0x9700, 1, 2, avr::decode_Rd_pair_K, avr::exec_SBIW},
    // IJMP (1001 0100 0000 1001)
    {0xFFFF, 0x9409, 1, 2, avr::decode_none, avr::exec_IJMP},
    // ICALL (1001 0101 0000 1001)
    {0xFFFF, 0x9509, 1, 3, avr::decode_none, avr::exec_ICALL},
    // MOVW (0000 0001 dddd rrrr)
    {0xFF00, 0x0100, 1, 1, avr::decode_Rd_Rr_pair, avr::exec_MOVW},
    // LPM Rd, Z+ (1001 000d dddd 0101)
    {0xFE0F, 0x9005, 1, 3, avr::decode_Rd, avr::exec_LPM_Zplus},
    // ELPM Rd, Z+ (1001 010d dddd 0101)
    {0xFE0F, 0x9105, 1, 3, avr::decode_Rd, avr::exec_ELPM_Zplus}};

const std::size_t avr::avre_plus_instruction_table_size =
    sizeof(avre_plus_instruction_table) /
    sizeof(avre_plus_instruction_table[0]);
