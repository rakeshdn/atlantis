/*
 * Before using this library,
 * ----------------------------------------------------------------------
 * If backtrace() or  _Unwind_Backtrace() works on your platform,
 * and with your preferred build options, then you probably have no 
 * use for this library.
 *
 * Also thumb instructions are not supported.
 *
 * ----------------------------------------------------------------------
 * Function btrace_callstack attempts to back trace the call stack
 * even when frame pointer is omitted. It examines
 * each previous opcode that modified SP and reverses those changes
 * to compute the SP just before calling the current function.
 * Currently it relies on being able to deduce all SP changes from
 * immediate operands or counting number of registers being stored
 * on stack. Any code that manipulates SP by an amount determined
 * at runtime is likely to fail. That is, if it encounters a SP being
 * set to value of another register or from memory it fails.
 * The function uses the frame pointer if availabel to verify and
 * correct the computed SP if necessary.
 * One can attempt to make this function more robust by
 * 1. Not failing till we make sure there is no frame pointer,
 * 2. Walk to function epilogue to figure out how much the function
 * pops the stack.
 * ---------------------------------------------------------------------
 *
 * HOW TO BUILD THIS LIBRARY
 *
 * Change to directory btrace and run GNU make.
 *
 * makefile expects to find an environment variable named ARM_TOOL_PATH
 * that holds the path to location of your arm gcc bin folder.
 *
 * ----------------------------------------------------------------------
 *
 * HOW TO USE THIS LIBRARY
 *
 * ----------------------------------------------------------------------
 *
 * Client code has to include arm_btrace.h and link to libbtrace.a
 * If you get a linker error about undresolved symbol _etext  
 * you can either export the symbol appropriately from 
 * your linker script capturing location of end of text,
 * or you can work around this by defining the symbol
 * NO_ETEXT_IN_LINKER_SCRIPT while building libbtrace.a
 * 
 * Two scenarios are described below.
 *
 * 1. Explicit call to btrace_callstack in an error path before
 *    an exception is triggered.
 *    This is used to test whether this library works on your target
 *    before tinkering with exception handlers.
 *
 *    void foo(void)
 *    {
 *    	USE_BTRACE_STACK_FRAME_MACROS;
 *
 *    	..............
 *    	.... code ....
 *    	..............
 *
 *    	if(rare_and_mysterious_error)
 *    	{
 *    		SAVE_LOCAL_STACK_FRAME;
			btrace_callstack(callback_function,<max number of frames to trace back>);
 *    	}
 *    }
 *    If "callback_function" (type TraceCallbackFnPtr) is not NULL, it is
 *    invoked once for each function frame in the call stack. If the callback function
 *    returns a negative number then tracing stops. Otherwise it goes on till max number
 *    of frames is reached or the top of the stack is reached.
 *    The callback function can format the arguments as required and save it some place
 *    where it can be accessed for further analysis.
 *    Once this data is transferred to a development station, passing the PC along with
 *    the binary image with symbols to "addr2line" utility can give the function name,
 *    file name and line number corresponding to each frame.
 *
 *    An alternative to specifying a callback_function is to use btrace_set_print_fn()
 *    to install a callback of type "TracePrintFnPtr" where the library code will
 *    format the strings and the call back function simply has to save the strings
 *    for analysis.
 *
 *    However, obtaining a call stack in this manner has limited use in production
 *    code because it can be applied only only when the programmer can anticipate
 *    what type of error state may occur.
 *
 * -----------------------------------------------------------------------------------------
 *
 * 2. Generating a stack trace after an exception.
 *
 *    This is expected to be where this library is most useful.
 *    In some case after the exception handler runs, code can continue executing
 *    from the instruction that generated it. In other cases, it may be such that
 *    the handler just wants to save as much information and then reset the processor.
 *    Getting a stack trace from within the exception mode is not straight forward
 *    because the we do not have access to the SP register of the mode from which
 *    the exception was triggered. One way is for the handler to switch processor
 *    mode back to old mode, copy SP to memory and switch mode back and
 *    continue with the handler.Or we may complete the handler and then return to
 *    a special hook function in user mode which does the stack trace. Here we
 *    use the latter technique with the expectation that it interferes least with
 *    existing exception handler code, and is perhaps more portable across different
 *    versions of ARM architecture.
 *
 *    It is expected that during initialization,
 *    btrace_set_print_fn() was called to install a "put string"
 *    style fucntion.
 *
 *    To generate stack trace,
 *    Once exception handler decides to abort the process, it should
 *    set "Exception_Hook_LR" value as the final function (infinite loop)
 *    or reset vector, and then set its own return address to "exceptionHandlerReturnHookPtr"
 *    and then return from the handler.
 *    The print function is called once per frame being traced back.
 *    On completion of trace, exceptionHandlerReturnHook branches to
 *    "Exception_Hook_LR".
 *
 * An example trace output is given below.
 * 
 * # 0: PC = 00000a7c, SP_HI 000cd854, SP_LO  000cd850
 * # 1: PC = 00000bb8, SP_HI 000cd864, SP_LO  000cd858
 * # 2: PC = 00000d00, SP_HI 000cd894, SP_LO  000cd868
 * # 3: PC = 00000964, SP_HI 000cd89c, SP_LO  000cd898
 * # 4: PC = 0000e388, SP_HI 000cd8ac, SP_LO  000cd8a0
 *
 * PC value is the address of a branch instruction (callee's LR-4) 
 * and SP_HI to SP_LO is the data on stack used by this function (caller). 
 * It is trivial to modify the code to print all values between addresses 
 * SP_HI and SP_LO (both addresses inclusive).
 * PC values can be passed as one of the inputs to addr2line
 * to get the function name and line number.
 *---------------------------------------------------------------------------------------------
 */
