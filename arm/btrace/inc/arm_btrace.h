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

#ifndef _ARM_BTRACE_H_
#define _ARM_BTRACE_H_

/*
 *  See readme.txt for information on how to build and use this library.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char bt_uint8_t;
typedef unsigned int bt_uint32_t;

/*closest 4 byte aligned value that is >= _n */
#define ALIGN_4BYTES(_n)  (((_n) + 3) & ~3)

/*inline assembly to read register*/
#define  ARM_SP_READ(_val) __asm__ __volatile__ (" MOV   %0,sp":  "=r"  (_val))
#define  ARM_FP_READ(_val) __asm__ __volatile__ (" MOV   %0,r11": "=r"  (_val))
#define  ARM_PC_READ(_val) __asm__ __volatile__ (" MOV   %0,pc":  "=r"  (_val))
#define  ARM_LR_READ(_val) __asm__ __volatile__ (" MOV   %0,lr":  "=r"  (_val))
/*note: "_val" must not resolve to a function call when reading LR*/

/*code that call back function can return
 * to stop the trace before maxFrames are processed */
#define STOP_BTRACE  -10

/* status code indicating that we have reached the entry function
  which is the end of the callstack.*/
#define END_OF_STACK -11

/* Separate 1 KB stack used by backtrace exception return hook
 * Around 690 bytes of stack gets used if sprintf if called so anything
 * less than that, and this stops working*/
#define BTRACE_HOOK_STACK_SIZE 0x400

/* Note: The order of the fields in bt_stackframe_t can become
 * important if we wish to load /store all fields in a single
 * instruction as the registers with lower ids are stored
 * at lower addresses.*/
typedef struct StackFrame {
	bt_uint32_t fp; /* frame pointer r11         */
	bt_uint32_t sp; /* stack pointer             */
	bt_uint32_t lr; /* last return address       */
	bt_uint32_t pc; /* program counter           */
} bt_stackframe_t;

extern bt_uint32_t*  Exception_LR;
extern bt_uint32_t*  Exception_Hook_LR;
/*returns address of a global stack frame object*/
extern bt_stackframe_t* get_frame_ptr(void);

/*declare this macro at start of any function using SAVE_LOCAL_STACK_FRAME*/
#define USE_BTRACE_STACK_FRAME_MACROS \
	bt_stackframe_t* __sf_ptr

/*macro to read FP,SP,LR and PC of local stack and store into _pst */
#define SAVE_LOCAL_STACK_FRAME \
   __sf_ptr = get_frame_ptr();\
   ARM_FP_READ(__sf_ptr->fp);\
   ARM_SP_READ(__sf_ptr->sp);\
   ARM_LR_READ(__sf_ptr->lr);\
   ARM_PC_READ(__sf_ptr->pc)

/* Use arm-none-eabi-addr2line.exe -f -e <executable> <program address value>
 * to get the function names,filename and line number*/
typedef int (*TraceCallbackFnPtr)( int frameIndex, bt_stackframe_t* pFrame, bt_uint32_t* sp_high);

extern int btrace_callstack( TraceCallbackFnPtr callback_fn, int maxFrames );

/* Following functions are for printing callstack using a user specified TracePrintFnPtr type
 * when an abort happens. To do this, modifying the abort handler to update Exception_LR_Ptr with
 * the value of LR seen by it. then jump to exceptionHandlerReturnHook.
 * The exception handler can specify where the hook returns by setting global variable
 * "Exception_Hook_LR". If Exception_Hook_LR is not set then exceptionHandlerReturnHook
 * will jump to 0 by default which is the reset vector.
 * Note: Depending on the scenario we may want to return to the exception site
 * after handling the exception in which case there is no need to
 * set Exception_LR_Ptr or jump to exceptionHandlerReturnHook.*/

typedef int (*TracePrintFnPtr)(char* message);

/*returns existing TracePrintFnPtr or NULL*/
extern TracePrintFnPtr btrace_set_print_fn( TracePrintFnPtr print_fn, int maxFrames ) __attribute__ ((noinline));

extern void exceptionHandlerReturnHook(void) __attribute__ ((naked));


#ifdef __cplusplus
} /*extern C */
#endif


#endif /*_ARM_BTRACE_H_*/
