/*
 * The MIT License
 *
 * Copyright (c) 2013 Rakesh D Nair
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef _ARM_DEFS_H_
#define _ARM_DEFS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* numeric index of ARM general purpose registers*/
#define R00_ID   0
#define R01_ID   1
#define R02_ID   2
#define R03_ID   3
#define R04_ID   4
#define R05_ID   5
#define R06_ID   6
#define R07_ID   7
#define R08_ID   8
#define R09_ID   9
#define SB_ID    9
#define R10_ID   10
#define SL_ID    10
#define FP_ID    11
#define IP_ID    12
#define SP_ID    13
#define LR_ID    14
#define PC_ID    15

/*control bits in instruction */
#define I_BIT    0x02000000 /* has immediate operand*/
#define P_BIT    0x01000000 /* pre / post increment*/
#define U_BIT    0x00800000 /* up / down*/
#define S_BIT    0x00400000 /* copy status (SPSR to CPSR) */
#define B_BIT    0x00400000 /* ? */
#define W_BIT    0x00200000 /* write back to base register*/
#define L_BIT    0x00100000 /* load / store */

/*-------*---------------*-----------------*
 *| cccc | x | x | I | P | U | S/B | W | L |
 --------*---------------*-----------------*/
/*bit masks*/
#define OPCODE_DST_MASK    0x0000F000
#define OPCODE_SRC_MASK    0x000F0000
#define OPCODE_COND_MASK   0xF0000000
#define OPCODE_BR_OFFSET   0x000FFFFF

/* bits indicating registers for instructions
 * that use a list of registers as operand*/
#define OPCODE_R0_BIT      0x00000001
#define OPCODE_R1_BIT      0x00000002
#define OPCODE_R2_BIT      0x00000004
#define OPCODE_R3_BIT      0x00000008
#define OPCODE_R4_BIT      0x00000010
#define OPCODE_R5_BIT      0x00000020
#define OPCODE_R6_BIT      0x00000040
#define OPCODE_R7_BIT      0x00000080
#define OPCODE_R8_BIT      0x00000100
#define OPCODE_R9_BIT      0x00000200// SB_BIT
#define OPCODE_SL_BIT      0x00000400
#define OPCODE_FP_BIT      0x00000800
#define OPCODE_IP_BIT      0x00001000
#define OPCODE_SP_BIT      0x00002000
#define OPCODE_LR_BIT      0x00004000
#define OPCODE_PC_BIT      0x00008000

/*helper macro to compare bits specified by the mask to those in expected value*/
#define _IS_PART_EQUAL(_value,_andmask,_expected) (((_value) & (_andmask)) == (_expected))

/*helper macros to identify type of instruction */
#define IS_ADD(_opcode)                  _IS_PART_EQUAL(_opcode, 0x0D800000, 0x00800000)  /* cccc 00i0 100s xxxx xxxx xxxx xxxx xxxx*/
#define IS_SUB(_opcode)                  _IS_PART_EQUAL(_opcode, 0x0D400000, 0x00400000)  /* cccc 00i0 010s xxxx xxxx xxxx xxxx xxxx*/
#define IS_MOV(_opcode)                  _IS_PART_EQUAL(_opcode, 0x0DA00000, 0x01A00000)  /* cccc 00i1 101s xxxx xxxx xxxx xxxx xxxx*/
#define IS_SINGLE_DATA(_opcode)          _IS_PART_EQUAL(_opcode, 0x0C000000, 0x04000000)  /* cccc 010p ubwl xxxx xxxx xxxx xxxx xxxx*/
#define IS_BLOCK_DATA(_opcode)           _IS_PART_EQUAL(_opcode, 0x0E000000, 0x08000000)  /* cccc 100p uswl xxxx xxxx xxxx xxxx xxxx*/
#define IS_LDC_STC(_opcode)				 _IS_PART_EQUAL(_opcode, 0x0E000000, 0x0C000000)  /* cccc 110p unwl xxxx xxxx xxxx xxxx xxxx*/
#define IS_BRANCH(_opcode)               _IS_PART_EQUAL(_opcode, 0x0E000000, 0x0A000000)
#define IS_BRANCH_EXCHANGE(_opcode,_sts) _IS_PART_EQUAL(_opcode, 0x0FFFFF10, 0x012FFF10)

/*helper macros to identify operand registers, offsets and shifts*/
#define IS_LDST_DST_REG(_opcode,_regid)  ((((_opcode) >> 16) & 0xF) == (_regid))
#define IS_LDST_REG2(_opcode,_regid)     ((((_opcode) >> 12) & 0xF) == (_regid))
#define IS_MOV_DST_REG(_opcode,_regid)   ((((_opcode) >> 12) & 0xF) == (_regid))   /* for add,sub and mov dest reg is the 4th nibble */
#define IS_OPERAND_REG(_opcode,_regid)   ((((_opcode) >> 16) & 0xF) == (_regid))
#define GET_OPERAND_SHIFT(_opcode)       (((_opcode) >> 8) & 0xF)
#define GET_IMMEDIATE_OPERAND(_opcode)   ((_opcode) & 0xFF)

/**/
#define IS_NOT_CONDITIONAL(_opcode) 	 _IS_PART_EQUAL(_opcode, OPCODE_COND_MASK,0xE0000000)
#define IS_IMMEDIATE(_opcode)            _IS_PART_EQUAL(_opcode, I_BIT, I_BIT) /*  */
#define IS_WRITE_BACK(_opcode)      	 _IS_PART_EQUAL(_opcode, W_BIT, W_BIT) /* ! */
#define IS_LOAD(_opcode)            	 _IS_PART_EQUAL(_opcode, L_BIT, L_BIT) /* 1 = load, 0 = store */
#define IS_INCREMENT(_opcode)       	 _IS_PART_EQUAL(_opcode, U_BIT, U_BIT) /* 0 = decr, 1 = incr  */
#define IS_LOAD_PSR(_opcode)        	 _IS_PART_EQUAL(_opcode, S_BIT, S_BIT) /* ^ set cpsr = spsr*/
#define IS_PRE_INDEX(_opcode)       	 _IS_PART_EQUAL(_opcode, P_BIT, P_BIT) /* 1 = pre, 0 = post*/

/*PUSH is an unconditional STR or STM using SP as index register with write back*/
#define IS_PUSH(_opcode) ((((_opcode) & 0xFFFF0000) == 0xE92D0000) || \
			                 (((_opcode) & 0xFFFF0000) == 0xE52D0000) )


#ifdef __cplusplus
} /*extern C */
#endif

#endif /*_ARM_DEFS_H_*/
