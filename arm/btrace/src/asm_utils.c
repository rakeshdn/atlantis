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

#include "arm_btrace.h"

extern bt_stackframe_t* Exception_StackFrame_Ptr;
extern void exceptionTraceCallstack(void) __attribute__ ((noinline));

/*
 * Rated PG13 :)
 * A quick introduction to ARM exception and interrupt handling can be found here.
 * http://www.iti.uni-stuttgart.de/~radetzki/Seminar06/08_slides.pdf
 *
 * On entering an exception handler,we have the PC(+8) of the old mode
 * copied to the banked LR(r14) register, and old CPSR copied to SPSR_xxx
 * Exception handlers use their own stack.
 * To unwind stack of the thread that caused the exception,in addition to PC,
 * we need to know the old SP and LR (if it is not pushed on the stack).
 * To read these we have to either (1) Switch the mode back to the old mode,
 * copy these registers to memory and then switch the mode back and then do
 * a traceback, OR
 * (2) Instead of returning to the exception site after executing the handler,
 * go to a well known routine that will print the backtrace (executing in same
 * mode where exception happened) and then return to the original site or
 * take appropriate action like reset the board or enter an infinite loop.
 *
 * I have decided to go for the option 2 as it looks easiest to integrate
 * with the RTOS I am working with.
 *
 * a quick tutorial on arm gcc inline assembly can be found here,
 * http://www.ethernut.de/en/documents/arm-inline-asm.html
 * */

__attribute__((used)) bt_uint8_t   Exception_Hook_Stack[ALIGN_4BYTES(BTRACE_HOOK_STACK_SIZE)];
__attribute__((used)) bt_uint32_t* Exception_Hook_StackTop  = (bt_uint32_t*)&Exception_Hook_Stack[ALIGN_4BYTES(BTRACE_HOOK_STACK_SIZE)];
__attribute__((used)) bt_uint32_t* Exception_LR          = 0; /*LR seen by the exception handler*/
__attribute__((used)) bt_uint32_t* Exception_Hook_LR     = 0; /*address to jump to after trace hook, default value is reset vector*/
__attribute__((used)) bt_uint32_t* Exception_SP         = 0; /*ptr to original SP value at exception site*/

void exceptionHandlerReturnHook(void)/*this is a naked function*/
{
	/* <0> Jump to Label_HookFunctionStart which is start of code.
	 * Before that we have a few labels that hold the
	 * address of global variables and functions. The
	 * labels in assembly can be considered as kind of
	 * short range pointers (12 bit offset relative to PC)
	 * to global variables that are usually 32 bit pointers
	 * themselves.
	 * <1> Use IP(r12) to initialize a bt_stackframe_t object
	 * with current FP,SP,LR and exception LR to it.
	 * This assumes bt_stacktrace_t has the layout
	 * {FP,SP,LR,PC}; with FP at lowest address and PC
	 * at highest address.
	 * Note that PC in this frame is set to LR saved
	 * at Exception_LR_Ptr by the abort handler.
	 * <2> Jump to exceptionTraceCallstack which
	 * calls btrace_callstack(0,max_frames).
	 * <3> On return restore the old stack and then
	 * jump to address stored in Label_Exception_Hook_LR
	 * by the abort handler.*/
	__asm__ __volatile__ (
			"b Label_HookFunctionStart             \n\t"\
			"Label_Exception_Hook_StackTop:        \n\t"\
			".long Exception_Hook_StackTop         \n\t"\
			"Label_Exception_StackFrame_Ptr:       \n\t"\
			".long Exception_StackFrame_Ptr        \n\t"\
			"Label_Exception_LR_Ptr:               \n\t"\
			".long Exception_LR                    \n\t"\
			"Label_Exception_SP_Ptr:               \n\t"\
			".long Exception_SP                    \n\t"\
			"Label_Exception_Hook_LR_Ptr:          \n\t"\
			".long Exception_Hook_LR               \n\t"\
			"Label_HookFunctionStart:              \n\t" /*start of code*/                \
			"ldr ip, Label_Exception_SP_Ptr        \n\t"\
			"str sp, [ip]                          \n\t" /*(*Exception_SP_Ptr)=SP*/       \
			"ldr ip, Label_Exception_StackFrame_Ptr\n\t"\
			"ldr ip, [ip]                          \n\t"\
			"str fp, [ip]                          \n\t" /*Exception_StackFrame.FP=FP*/   \
			"str sp, [ip,#4]                       \n\t" /*Exception_StackFrame.SP=SP*/   \
			"str lr, [ip,#8]                       \n\t" /*Exception_StackFrame.LR=LR*/   \
			"ldr lr, Label_Exception_LR_Ptr        \n\t"\
			"ldr lr, [lr]                          \n\t"\
			"str lr, [ip,#12]                      \n\t" /*Exception_StackFrame.PC=(*Exception_LR_Ptr)*/ \
			"ldr sp, Label_Exception_Hook_StackTop \n\t"\
			"ldr sp, [sp]                          \n\t" /*SP = Exception_Hook_StackTop*/ \
			"bl exceptionTraceCallstack            \n\t" /*exceptionTraceCallstack()*/    \
			"ldr sp, Label_Exception_SP_Ptr        \n\t"\
			"ldr sp, [sp]                          \n\t" /*SP=(*Exception_SP_Ptr)*/       \
			"ldr ip, Label_Exception_Hook_LR_Ptr   \n\t" /*IP = Exception_Hook_LR */      \
			"ldr pc, [ip]                          \n\t" /*PC = *Exception_Hook_LR */     \
			"ldr pc, [pc, #-8]"                          /*unexpected!! loop forever PC=PC*/
	);
}

/*export a global function pointer for linking from assembly code*/
void (*exceptionHandlerReturnHookPtr)(void) = exceptionHandlerReturnHook;
