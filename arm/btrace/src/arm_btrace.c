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

#include "arm_defs.h"
#include "arm_btrace.h"


const bt_uint32_t FP_UPDATED_USING_SP   = 1;
const bt_uint32_t LR_FOUND_ON_STACK     = 2;
const bt_uint32_t OLD_FP_FOUND_ON_STACK = 4;

char tempPrintBuffer[128];
static int max_frames=0;
static TracePrintFnPtr trace_print_fn = 0;
static bt_stackframe_t Exception_StackFrame;
bt_stackframe_t* Exception_StackFrame_Ptr = &Exception_StackFrame;

extern void exceptionTraceCallstack(void) __attribute__ ((noinline));

bt_stackframe_t* get_frame_ptr() { return Exception_StackFrame_Ptr; }

int btrace_set_print_fn(TracePrintFnPtr print_fn,int maxFrames)
{
	trace_print_fn = print_fn;
	max_frames = maxFrames;
}

/*count the number of registers in register list in the opcode*/
int num_registers(bt_uint32_t opcode)
{
	int nbits,count=0;
	for(nbits = 0; nbits < 16; ++nbits)
	{
		if((opcode >> nbits) & 1)
		{
			++count;
		}
	}
	return count;
}

/*
 * ------------------------------------------------------------------------------
 * Stack walking without relying on the presence of frame pointer.
 * ------------------------------------------------------------------------------
 * It is assumed that the compiler generates code for a "full descending" stack.
 * That is, stack grows from higher to lower addresses and stack pointer is
 * pre decremented when anything is pushed.So SP always points to a used location.
 * -------------------------------------------------------------------------------
 * 0. Start at exception site. capture PC(-8),SP,FP,LR.
 * 1. Walk back to the start of the current function.
 * 2. One each instruction undo decrements made to SP to find SP at the call site invoking this function.
 *    Known limitations: If a value in another register was used to modify SP then we will have to go to
 *    the end of the function and look at the function epilogue for the amount by which stack is incremented,
 *    either by add or pop/ldr/ldm. However, in practice we may never encounter this condition as
 *    in vast majority of cases SP is incremented or decremented by an amount that can be deduced
 *    either as an immediate operand of a sub instruction, or from the number of registers being pushed.
 * 3. Call client specified call back function. If the callback function returns STOP_BTRACE then stop here.
 * 4. Update the working stack frame, if FP was pushed on the stack frame then previous stack pointer = FP + 4.
 *    otherwise use computed SP at start.
 * 4. Use LR - 4 as previous call site.
 * 5. Repeat steps 2 to 5 till max frames are processed.
 * -------------------------------------------------------------------------------
 * */


/* Return values,
 * > 0  indicates SP modified.
 * = 0 means no change to SP, and
 * < 0 indicate error.
 * */
static int process_instruction( bt_uint32_t opcode,
								bt_uint32_t* sp_ptr,
								bt_uint32_t* fp_on_stack_ptr,
								bt_uint32_t* lr_on_stack_ptr,
								bt_uint32_t* trace_flags_ptr)
{
	#ifdef DEBUG_ON_QEMU
	char message[100];
	#endif

	int retCode = 0;
	if(IS_SUB(opcode) || IS_ADD(opcode))
	{
		/*if the destination register is SP */
		if(IS_MOV_DST_REG(opcode,SP_ID))
		{
			if(IS_IMMEDIATE(opcode))
			{   /*add what was subtracted*/
				if(IS_SUB(opcode))
				{   /*add if opcode is sub*/
					*sp_ptr += (GET_IMMEDIATE_OPERAND(opcode) << (0xF & (16 - GET_OPERAND_SHIFT(opcode))) );
					retCode = __LINE__;
				}
				else
				{ 	/*sub if opcode is add*/
					/*we do not expect to see SP increments when walking back a
					  "full descending" stack. however, if in future, code is
					  added to walk forward to the end of the function this
					  may come in handy*/
					*sp_ptr -= (GET_IMMEDIATE_OPERAND(opcode) << (0xF & (16 - GET_OPERAND_SHIFT(opcode))) );
					retCode = __LINE__;
				}
			}
			else
			{
				retCode = -__LINE__; // cannot unwind
			}
		}
		else if(IS_MOV_DST_REG(opcode,FP_ID))
		{   /* FP is destination register */
			if( IS_ADD(opcode) && /*this is an add operation*/
				IS_OPERAND_REG(opcode,SP_ID) && /*with SP is one of the operands*/
		        IS_IMMEDIATE(opcode) ) /*and an immediate offset*/
			{   /* add	fp, sp, #offset*/
				*trace_flags_ptr |= FP_UPDATED_USING_SP;
			}
		}
	}
	else if(IS_SINGLE_DATA(opcode))
	{	/*push and pop of single register is also handled here*/
		/*LDR and STR always writeback when post indexed*/
		if(IS_LDST_DST_REG(opcode,SP_ID))
		{
			if(IS_WRITE_BACK(opcode))
			{   /*we are only interested if write back bit is set*/
				if(IS_INCREMENT(opcode))
				{
					#if 0 /*-O3 bug fix: ignore pops. when configured for full optimization (-O3)
					gcc generates pops in between function code in addition to function epilogue!*/

					/*we do not expect to see SP increments when walking back a
					  "full descending" stack. however, if in future, code is
					  added to walk forward to the end of the function this
					  may come in handy*/
					*sp_ptr -= sizeof(bt_uint32_t);/*single word transfer*/
					retCode = __LINE__;
					#endif

				}
				else
				{
					*sp_ptr += sizeof(bt_uint32_t);/*single word transfer*/
					if(IS_LDST_REG2(opcode,LR_ID))
					{
					   *lr_on_stack_ptr = sp_ptr[-1];
					   *trace_flags_ptr |= LR_FOUND_ON_STACK;
					}
					if(IS_LDST_REG2(opcode,FP_ID))
					{  /*if lr was pushed then it is pushed before FP*/
					   *fp_on_stack_ptr = sp_ptr[-1];
					   *trace_flags_ptr |= OLD_FP_FOUND_ON_STACK;
					}
					retCode = __LINE__;
				}
			}
		}
	}
	else if(IS_BLOCK_DATA(opcode))
	{/*push and pop of multiple registers is also handled here*/
		if(IS_LDST_DST_REG(opcode,SP_ID))
		{
			if(IS_WRITE_BACK(opcode))
			{
				int num_reg = num_registers(opcode);
				if(IS_INCREMENT(opcode))
				{
					#if 0 /*-O3 bug fix: ignore pops. when configured for full optimization (-O3)
					gcc generates pops in between function code in addition to function epilogue!*/
					/*we do not expect to see SP increments when walking back a
					  "full descending" stack. however, if in future, code is
					  added to walk forward to the end of the fucntion this
					  may come in handy*/
					*sp_ptr -= (num_reg << 2);
					retCode = __LINE__;
					#endif
				}
				else
				{
					*sp_ptr += (num_reg << 2);// 4 bytes per register.
					if(opcode & OPCODE_LR_BIT)
					{
						*lr_on_stack_ptr = ((bt_uint32_t*)(*sp_ptr))[-1];
						*trace_flags_ptr |= LR_FOUND_ON_STACK;
					}
					if(opcode & OPCODE_FP_BIT)
					{ /*if lr was pushed then it is pushed before FP*/
						*fp_on_stack_ptr = (opcode & OPCODE_LR_BIT)? \
								           ((bt_uint32_t*)(*sp_ptr))[-2] :\
								           ((bt_uint32_t*)(*sp_ptr))[-1] ;
						*trace_flags_ptr |= OLD_FP_FOUND_ON_STACK;

					}
					retCode = __LINE__;
				}
			}
		}
	}
	else if(IS_LDC_STC(opcode))
	{   /* TODO: handling of load store coprocessor registers on stack
	       using vldmia,vpush etc. remains to be tested.*/

		/* vpush, vpop, vldmia */
		if(IS_LDST_DST_REG(opcode,SP_ID))
		{
			if(IS_WRITE_BACK(opcode))
			{
				if(IS_INCREMENT(opcode))
				{
					#if 0 /*-O3 bug fix: ignore pops. when configured for full optimization (-O3)
					gcc generates pops in between function code in addition to function epilogue!*/

					/*we do not expect to see SP increments when walking back a
					  "full descending" stack. however, if in future, code is
					  added to walk forward to the end of the fucntion this
					  may come in handy*/
					*sp_ptr -= ((GET_IMMEDIATE_OPERAND(opcode))<<2);
					retCode = __LINE__;
					#endif
				}
				else
				{
					*sp_ptr += ((GET_IMMEDIATE_OPERAND(opcode))<<2);
					retCode = __LINE__;
				}
			}
		}
	}

	#ifdef DEBUG_ON_QEMU
    sprintf(message,"opcode = %x, SP= %x, ret = %d\r\n", opcode, *sp_ptr, retCode);
    _write(2,message,strlen(message));
	#endif

    return retCode;
}

#ifndef NO_ETEXT_IN_LINKER_SCRIPT
/*end of .text exported from ld script */
extern unsigned long _etext;
#endif

/*assumes that all functions begin with a push statement*/
static int walk_to_fn_start(bt_uint32_t** fn_start_pc_ptr,
							bt_uint32_t* fn_start_sp_ptr,
							bt_uint32_t* fp_on_stack_ptr,
							bt_uint32_t* lr_on_stack_ptr,
							bt_uint32_t* trace_flags_ptr)
{
	int status = 0;
	/*while not push decrement pc and revert SP changes*/
	while (!IS_PUSH(**fn_start_pc_ptr))
	{/*push single or push multiple*/
		status = process_instruction( **fn_start_pc_ptr,
				                      fn_start_sp_ptr,
				                      fp_on_stack_ptr,
				                      lr_on_stack_ptr,
				                      trace_flags_ptr);
		/*todo: add sanity check here, if we see pop on the way to start of functions then some thing is fishy */
		/*if(status < 0) */
		*fn_start_pc_ptr -= 1;

	    #ifndef NO_ETEXT_IN_LINKER_SCRIPT

		if(*fn_start_pc_ptr >= (bt_uint32_t*)&_etext)
		{   /*STOP !! _etext is expected to be exported by linker script
		     This check is just to protect against this function looping forever in
		     non code sections searching for a push.
		     If you get _etext unresolved error then define NO_ETEXT_IN_LINKER_SCRIPT
		     when compiling this file*/
			status = -__LINE__;
		    break;
		}

		#endif /*NO_ETEXT_IN_LINKER_SCRIPT*/
	}
	if(status >= 0)
	{
		/*process the push */
		status = process_instruction( **fn_start_pc_ptr,
									  fn_start_sp_ptr,
									  fp_on_stack_ptr,
									  lr_on_stack_ptr,
									  trace_flags_ptr);
	}
	/*if(status < 0) */
return status;
}

extern bt_stackframe_t* Exception_Frame_Ptr;

static int process_frame(int frameIndex, bt_stackframe_t* frame_ptr, TraceCallbackFnPtr trace_callback_fn )
{
	 int status = -1;
	 int trace_func_ret = 0;

	 #ifdef DEBUG_ON_QEMU
	 char message[200];
	 #endif

	 /*pc read is always PC+8 on arm, so */
	 bt_uint32_t* fn_start_pc_ptr = (bt_uint32_t *)(frame_ptr->pc);
	 bt_uint32_t fn_start_sp = frame_ptr->sp;
	 bt_uint32_t fp_on_stack = 0;
	 bt_uint32_t lr_on_stack = 0;
	 bt_uint32_t trace_flags = 0;

	 status = walk_to_fn_start( &fn_start_pc_ptr,
			                    &fn_start_sp,
			                    &fp_on_stack,
			                    &lr_on_stack,
			                    &trace_flags);

	 #ifdef DEBUG_ON_QEMU
	 sprintf(message,"start_pc= %x, start_sp = %u, prev fp = %u, prev lr = %u, flags = %u \r\n",
			          fn_start_pc_ptr, fn_start_sp, fp_on_stack, lr_on_stack, trace_flags);
	 _write(2,message,strlen(message));
	 #endif

	 if(status >= 0)
	 {
		 if((trace_flags & FP_UPDATED_USING_SP) && frame_ptr->fp)
		 {   /* A sanity check.If this function updated FP, then FP
		        must point to SP pointing to first
		        thing that was pushed on the stack by this function.
		        fn_start_sp points to SP on entry before anything was pushed.*/
			 /* frame_ptr->fp == Exception_Frame_Ptr is a special case
			  * Exception_Frame_Ptr must be ignored. */

			 if(frame_ptr->fp != (fn_start_sp - sizeof(bt_uint32_t)))
			 {
				/* it is likely that the frame pointer is correct
				 * and fn_start_sp computed is wrong. so update
				 * fn_start_sp */

				#ifdef DEBUG_ON_QEMU
				sprintf(message,"warning: FP= %u, SP on entry = %u, mismatch\r\n", frame_ptr->fp, fn_start_sp);
				_write(2,message,strlen(message));
				#endif

				/*fn_start_sp*/
				fn_start_sp = (frame_ptr->fp + sizeof(bt_uint32_t));
			 }
		 }

		 if(trace_callback_fn)
		 {
			 /* invoke call back with frame and top of stack frame,
			  * SP High passed to the trace function is first address used
			  * by this function.hence the -4 */
			 trace_func_ret = trace_callback_fn( frameIndex, frame_ptr,
												 (bt_uint32_t*)(fn_start_sp - sizeof(bt_uint32_t)));
		 }
		 else if(trace_print_fn)
		 {
			 sprintf(tempPrintBuffer,"#%02d: PC= %x, SP= %x, LR= %x, FP= %x, callerSP= %x\n", frameIndex,
					 frame_ptr->pc, frame_ptr->sp, frame_ptr->lr, frame_ptr->fp, fn_start_sp);
			 trace_func_ret = trace_print_fn(tempPrintBuffer);

			 /*for( tempSp = (bt_uint32_t*)(fn_start_sp - sizeof(bt_uint32_t));
			      ((tempSp >= (bt_uint32_t*)(frame_ptr->sp)) && (trace_func_ret >= 0));
			      tempSp -= sizeof(bt_uint32_t))
			 {
				 sprintf(tempPrintBuffer,"\t@%x: %x \n",tempSp,*tempSp);
				 trace_func_ret = trace_print_fn(tempPrintBuffer);
			 }*/
		 }

		 if(trace_func_ret == STOP_BTRACE)
		 {	/*call back function requested a stop here*/
			 return status;
		 }

		 /*update the frame pointer to that at call site for this function*/
		 frame_ptr->sp = fn_start_sp;

		 if(trace_flags & OLD_FP_FOUND_ON_STACK)
		 {	 /*leave frame_ptr->fp unmodified is this function did not update it*/
			 frame_ptr->fp = fp_on_stack;
		 }

		 if(trace_flags & LR_FOUND_ON_STACK)
		 {   /*leave frame_ptr->lr unmodified is this function did not push it*/
			 /*however if this function did push LR onto the stack then we must
			   use that value as LR may have been changed by a branch from this function */
			 frame_ptr->lr = lr_on_stack;

			 if(0 == lr_on_stack)
			 {	 /* if lr was pushed onto stack but it is zero,this is probably the
			        thread entry function, so stop walking. */
				 return END_OF_STACK;
			 }
		 }
		 /*update stack frame for next iteration.*/
		 frame_ptr->pc = (frame_ptr->lr - sizeof(bt_uint32_t));
	 }
return status;
}

int btrace_callstack(TraceCallbackFnPtr callback_fn, int maxFrames)
{
	 int frameCount = 0;
	 /* iterate through each stack frame, till we hit an error or
	  * maxFrames is reached */
	 while(frameCount < maxFrames)
	 {
		 if(0 < process_frame(frameCount,get_frame_ptr(),callback_fn))
		 {
			++frameCount;
		 }
		 else
		 {
			 break;
		 }
	 }
	 return frameCount;
}


/*call btrace_callstack passing in Exception_Frame_Ptr*/
void exceptionTraceCallstack(void)
{
	btrace_callstack(0,max_frames);
}


