# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: crt0.s 1066 2005-04-29 10:25:23Z pixel $
# Standard startup file.


.set noat
.set noreorder

.global _start
.global	_exit

	# Support for _init() and _fini().
	.global _init
	.global _fini
	.type	_init, @function
	.type	_fini, @function

	# The .weak keyword ensures there's no error if
	# _init/_fini aren't defined.
	.weak	_init
	.weak	_fini

	.extern	_heap_size
	.extern	_stack
	.extern _stack_size

	.text

	.ent _start
_start:

addiu $sp, $sp, -0x20
sq $ra, 0x00($sp)
sq $gp, 0x10($sp)

jal main
nop

lq $ra, 0x00($sp)
lq $gp, 0x10($sp)
jr $ra
addiu $sp, $sp, 0x20
