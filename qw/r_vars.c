// r_vars.c: global refresh variables

#include <u.h>
#include <libc.h>
#include <stdio.h>
#include	"quakedef.h"

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

int	r_bmodelactive;
