;    threadsISR.asm    08/07/2015
;*******************************************************************************
;  STATE        Task Control Block        Stacks (malloc'd)
;                          ______                                       T0 Stack
;  Running tcbs[0].thread | xxxx-+------------------------------------->|      |
;                 .stack  | xxxx-+-------------------------------       |      |
;                 .block  | 0000 |                               \      |      |
;                 .retval |_0000_|                       T1 Stack \     |      |
;  Ready   tcbs[1].thread | xxxx-+---------------------->|      |  \    |      |
;                 .stack  | xxxx-+------------------     |      |   \   |      |
;                 .block  | 0000 |                  \    |      |    -->|(exit)|
;                 .retval |_0000_|        T2 Stack   --->|r4-r15|       |------|
;  Blocked tcbs[2].thread | xxxx-+------->|      |       |  SR  |
;                 .stack  | xxxx-+---     |      |       |  PC  |
;                 .block  |(sem) |   \    |      |       |(exit)|
;                 .retval |_0000_|    --->|r4-r15|       |------|
;  Free    tcbs[3].thread | 0000 |        |  SR  |
;                 .stack  | ---- |        |  PC  |
;                 .block  | ---- |        |(exit)|
;                 .retval |_----_|        |------|
;
;*******************************************************************************

            .cdecls C,"msp430.h"            ; MSP430
            .cdecls C,"pthreads.h"          ; threads header

            .def    TA_isr
            .ref    ctid
            .ref    tcbs
            ;.ref	MAX_THREADS

tcb_thread  .equ    (tcbs + 0)
tcb_stack   .equ    (tcbs + 2)
tcb_block   .equ    (tcbs + 4)

; Code Section ------------------------------------------------------------

            .text                           ; beginning of executable code
; Timer A ISR -------------------------------------------------------------
TA_isr:     bic.w   #TAIFG|TAIE,&TACTL      ; acknowledge & disable TA interrupt
; >>>>>> 1. Save current context on stack
			push	r15
			push	r14
			push	r13
			push	r12
			push	r11
			push	r10
			push	r9
			push	r8
			push	r7
			push	r6
			push	r5
			push	r4
; >>>>>> 2. Save SP in task control block
			mov.w	&ctid, r5
			add.w	r5, r5
			add.w	r5, r5
			add.w	r5, r5
			add.w	#2, r5
			mov.w	SP, @r5(tcbs)
; >>>>>> 3. Find next non-blocked thread tcb
			mov.w	&ctid, r4
next_thred:	inc.w	r4
			cmp		#4, r4
			jeq		wrap
			mov.w	r4, r5
			add.w	r5, r5
			add.w	r5, r5
			add.w	r5, r5
			cmp.w	#0, @r5(tcbs)
			jeq		next_thred
			add.w	#4, r5
			cmp.w	#0, @r5(tcbs)
			jeq		found_one
			cmp		&ctid, r4
			jeq		sleep
			jmp		next_thred
wrap:		mov.w	#-1, r4
			jmp		next_thred
; >>>>>> 4. If all threads blocked, enable interrupts in LPM0
sleep:		bis.w	#LPM0,SR				; goto sleep
			jmp		next_thred
; >>>>>> 5. Set new SP
found_one:	sub.w	#2, r5
			mov.w	@r5(tcbs), SP
			mov.w	r4, &ctid
; >>>>>> 6. Load context from stack
			pop		r4
			pop		r5
			pop		r6
			pop		r7
			pop		r8
			pop		r9
			pop		r10
			pop		r11
			pop		r12
			pop		r13
			pop		r14
			pop		r15
            bis.w    #TAIE,&TACTL           ; enable TA interrupts
            reti


; Interrupt Vector --------------------------------------------------------
            .sect   ".int08"                ; timer A section
            .word   TA_isr                  ; timer A isr
            .end
