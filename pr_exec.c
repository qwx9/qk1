#include "quakedef.h"

#define	LOCALSTACK_SIZE		32768
static int			localstack[LOCALSTACK_SIZE];
static int			localstack_used;

static const char *pr_opnames[] =
{
"DONE",

"MUL_F",
"MUL_V",
"MUL_FV",
"MUL_VF",

"DIV",

"ADD_F",
"ADD_V",

"SUB_F",
"SUB_V",

"EQ_F",
"EQ_V",
"EQ_S",
"EQ_E",
"EQ_FNC",

"NE_F",
"NE_V",
"NE_S",
"NE_E",
"NE_FNC",

"LE",
"GE",
"LT",
"GT",

"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",

"ADDRESS",

"STORE_F",
"STORE_V",
"STORE_S",
"STORE_ENT",
"STORE_FLD",
"STORE_FNC",

"STOREP_F",
"STOREP_V",
"STOREP_S",
"STOREP_ENT",
"STOREP_FLD",
"STOREP_FNC",

"RETURN",

"NOT_F",
"NOT_V",
"NOT_S",
"NOT_ENT",
"NOT_FNC",

"IF",
"IFNOT",

"CALL0",
"CALL1",
"CALL2",
"CALL3",
"CALL4",
"CALL5",
"CALL6",
"CALL7",
"CALL8",

"STATE",

"GOTO",

"AND",
"OR",

"BITAND",
"BITOR"
};

//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
static void
PR_PrintStatement(pr_t *pr, dstatement_t *s)
{
	int		i;

	if ( (unsigned)s->op < sizeof(pr_opnames)/sizeof(pr_opnames[0]))
	{
		Con_Printf ("%s ",  pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i<10 ; i++)
			Con_Printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf ("%sbranch %d",PR_GlobalString(pr, s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		Con_Printf ("branch %d",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Printf ("%s",PR_GlobalString(pr, s->a));
		Con_Printf ("%s", PR_GlobalStringNoContents(pr, s->b));
	}
	else
	{
		if (s->a)
			Con_Printf ("%s",PR_GlobalString(pr, s->a));
		if (s->b)
			Con_Printf ("%s",PR_GlobalString(pr, s->b));
		if (s->c)
			Con_Printf ("%s", PR_GlobalStringNoContents(pr, s->c));
	}
	Con_Printf ("\n");
}

/*
============
PR_StackTrace
============
*/
static void
PR_StackTrace(pr_t *pr)
{
	dfunction_t	*f;
	int			i;

	if (pr->depth == 0)
	{
		Con_Printf ("<NO STACK>\n");
		return;
	}

	pr->stack[pr->depth].f = pr->xfunction;
	for (i=pr->depth ; i>=0 ; i--)
	{
		f = pr->stack[i].f;

		if(!f){
			Con_Printf ("<NO FUNCTION>\n");
		}else
			Con_Printf ("%12s : %s\n", PR_Str(pr, f->s_file), PR_Str(pr, f->s_name));
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f (cmd_t *c)
{
	dfunction_t	*f, *best;
	int			max;
	int			num;
	int			i;

	USED(c);
	if (!sv.active)
		return;

	num = 0;
	do
	{
		max = 0;
		best = nil;
		for (i=0 ; i<sv.pr->numfunctions ; i++)
		{
			f = &sv.pr->functions[i];
			if (f->profile > max)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < 10)
				Con_Printf ("%7d %s\n", best->profile, PR_Str(sv.pr, best->s_name));
			num++;
			best->profile = 0;
		}
	} while (best);
}


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError (pr_t *pr, char *fmt, ...)
{
	va_list arg;
	char s[1024];

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	PR_PrintStatement(pr, pr->statements + pr->xstatement);
	PR_StackTrace(pr);
	Con_Printf("%s\n", s);

	pr->depth = 0;	// dump the stack so host_error can shutdown functions
	Host_Error("Program error");
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
static int
PR_EnterFunction(pr_t *pr, dfunction_t *f)
{
	int		i, j, c, o;

	pr->stack[pr->depth].s = pr->xstatement;
	pr->stack[pr->depth].f = pr->xfunction;
	pr->depth++;
	if(pr->depth >= MAX_STACK_DEPTH)
		PR_RunError (pr, "stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError(pr, "PR_ExecuteProgram: locals stack overflow\n");

	for (i=0 ; i < c ; i++)
		localstack[localstack_used+i] = ((int *)pr->globals)[f->parm_start + i];
	localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)pr->globals)[o] = ((int *)pr->globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	pr->xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
static int
PR_LeaveFunction(pr_t *pr)
{
	int		i, c;

	if (pr->depth <= 0)
		fatal ("prog stack underflow");

	// restore locals from the stack
	c = pr->xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError (pr, "PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr->globals)[pr->xfunction->parm_start + i] = localstack[localstack_used+i];

	// up stack
	pr->depth--;
	pr->xfunction = pr->stack[pr->depth].f;
	return pr->stack[pr->depth].s;
}


/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteProgram (pr_t *pr, func_t fnum)
{
	eval_t	*a, *b, *c;
	int			s;
	dstatement_t	*st;
	dfunction_t	*f, *newf;
	int		runaway;
	int		i;
	edict_t	*ed;
	int		exitdepth;
	eval_t	*ptr;

	if (!fnum || fnum >= pr->numfunctions)
	{
		if (pr->global_struct->self)
			ED_Print (pr, PROG_TO_EDICT(pr, pr->global_struct->self));
		Host_Error ("PR_ExecuteProgram: NULL function");
	}

	f = &pr->functions[fnum];

	runaway = 100000;
	pr->trace = false;

	// make a stack frame
	exitdepth = pr->depth;

	s = PR_EnterFunction (pr, f);

	while (1)
	{
		s++;	// next statement

		st = &pr->statements[s];
		a = (eval_t *)&pr->globals[(unsigned short)st->a];
		b = (eval_t *)&pr->globals[(unsigned short)st->b];
		c = (eval_t *)&pr->globals[(unsigned short)st->c];

		if (!--runaway)
			PR_RunError (pr, "runaway loop error");

		pr->xfunction->profile++;
		pr->xstatement = s;

		if (pr->trace)
			PR_PrintStatement (pr, st);

		switch (st->op)
		{
		case OP_ADD_F:
			c->_float = a->_float + b->_float;
			break;
		case OP_ADD_V:
			c->vector[0] = a->vector[0] + b->vector[0];
			c->vector[1] = a->vector[1] + b->vector[1];
			c->vector[2] = a->vector[2] + b->vector[2];
			break;

		case OP_SUB_F:
			c->_float = a->_float - b->_float;
			break;
		case OP_SUB_V:
			c->vector[0] = a->vector[0] - b->vector[0];
			c->vector[1] = a->vector[1] - b->vector[1];
			c->vector[2] = a->vector[2] - b->vector[2];
			break;

		case OP_MUL_F:
			c->_float = a->_float * b->_float;
			break;
		case OP_MUL_V:
			c->_float = a->vector[0]*b->vector[0]
					+ a->vector[1]*b->vector[1]
					+ a->vector[2]*b->vector[2];
			break;
		case OP_MUL_FV:
			c->vector[0] = a->_float * b->vector[0];
			c->vector[1] = a->_float * b->vector[1];
			c->vector[2] = a->_float * b->vector[2];
			break;
		case OP_MUL_VF:
			c->vector[0] = b->_float * a->vector[0];
			c->vector[1] = b->_float * a->vector[1];
			c->vector[2] = b->_float * a->vector[2];
			break;

		case OP_DIV_F:
			c->_float = a->_float / b->_float;
			break;

		case OP_BITAND:
			c->_float = (int)a->_float & (int)b->_float;
			break;

		case OP_BITOR:
			c->_float = (int)a->_float | (int)b->_float;
			break;

		case OP_GE:
			c->_float = a->_float >= b->_float;
			break;
		case OP_LE:
			c->_float = a->_float <= b->_float;
			break;
		case OP_GT:
			c->_float = a->_float > b->_float;
			break;
		case OP_LT:
			c->_float = a->_float < b->_float;
			break;
		case OP_AND:
			c->_float = a->_float && b->_float;
			break;
		case OP_OR:
			c->_float = a->_float || b->_float;
			break;

		case OP_NOT_F:
			c->_float = !a->_float;
			break;
		case OP_NOT_V:
			c->_float = !a->vector[0] && !a->vector[1] && !a->vector[2];
			break;
		case OP_NOT_S:
			c->_float = !a->string || !*PR_Str(pr, a->string);
			break;
		case OP_NOT_FNC:
			c->_float = !a->function;
			break;
		case OP_NOT_ENT:
			c->_float = (PROG_TO_EDICT(pr, a->edict) == pr->edicts);
			break;

		case OP_EQ_F:
			c->_float = a->_float == b->_float;
			break;
		case OP_EQ_V:
			c->_float = (a->vector[0] == b->vector[0]) &&
						(a->vector[1] == b->vector[1]) &&
						(a->vector[2] == b->vector[2]);
			break;
		case OP_EQ_S:
			c->_float = !strcmp(PR_Str(pr, a->string), PR_Str(pr, b->string));
			break;
		case OP_EQ_E:
			c->_float = a->_int == b->_int;
			break;
		case OP_EQ_FNC:
			c->_float = a->function == b->function;
			break;

		case OP_NE_F:
			c->_float = a->_float != b->_float;
			break;
		case OP_NE_V:
			c->_float = (a->vector[0] != b->vector[0]) ||
						(a->vector[1] != b->vector[1]) ||
						(a->vector[2] != b->vector[2]);
			break;
		case OP_NE_S:
			c->_float = strcmp(PR_Str(pr, a->string), PR_Str(pr, b->string));
			break;
		case OP_NE_E:
			c->_float = a->_int != b->_int;
			break;
		case OP_NE_FNC:
			c->_float = a->function != b->function;
			break;

//==================
		case OP_STORE_F:
		case OP_STORE_ENT:
		case OP_STORE_FLD:		// integers
		case OP_STORE_S:
		case OP_STORE_FNC:		// pointers
			b->_int = a->_int;
			break;
		case OP_STORE_V:
			b->vector[0] = a->vector[0];
			b->vector[1] = a->vector[1];
			b->vector[2] = a->vector[2];
			break;

		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:		// integers
		case OP_STOREP_S:
		case OP_STOREP_FNC:		// pointers
			ptr = (eval_t *)((byte *)pr->edicts + b->_int);
			ptr->_int = a->_int;
			break;
		case OP_STOREP_V:
			ptr = (eval_t *)((byte *)pr->edicts + b->_int);
			ptr->vector[0] = a->vector[0];
			ptr->vector[1] = a->vector[1];
			ptr->vector[2] = a->vector[2];
			break;

		case OP_ADDRESS:
			ed = PROG_TO_EDICT(pr, a->edict);
#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			if (ed == (edict_t *)pr->edicts && sv.state == ss_active)
				PR_RunError(pr, "assignment to world entity");
			c->_int = (byte *)((int *)&ed->v + b->_int) - (byte *)pr->edicts;
			break;

		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC:
			ed = PROG_TO_EDICT(pr, a->edict);
#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			a = (eval_t *)((int *)&ed->v + b->_int);
			c->_int = a->_int;
			break;

		case OP_LOAD_V:
			ed = PROG_TO_EDICT(pr, a->edict);
#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			a = (eval_t *)((int *)&ed->v + b->_int);
			c->vector[0] = a->vector[0];
			c->vector[1] = a->vector[1];
			c->vector[2] = a->vector[2];
			break;

//==================

		case OP_IFNOT:
			if (!a->_int)
				s += st->b - 1;	// offset the s++
			break;

		case OP_IF:
			if (a->_int)
				s += st->b - 1;	// offset the s++
			break;

		case OP_GOTO:
			s += st->a - 1;	// offset the s++
			break;

		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8:
			pr->argc = st->op - OP_CALL0;
			if (!a->function)
				PR_RunError (pr, "NULL function");

			newf = &pr->functions[a->function];

			if (newf->first_statement < 0)
			{	// negative statements are built in functions
				i = -newf->first_statement;
				if (i >= pr->numbuiltins)
					PR_RunError (pr, "Bad builtin call number %d", i);
				pr->builtins[i](pr);
				break;
			}

			s = PR_EnterFunction (pr, newf);
			break;

		case OP_DONE:
		case OP_RETURN:
			pr->globals[OFS_RETURN] = pr->globals[(unsigned short)st->a];
			pr->globals[OFS_RETURN+1] = pr->globals[(unsigned short)st->a+1];
			pr->globals[OFS_RETURN+2] = pr->globals[(unsigned short)st->a+2];

			s = PR_LeaveFunction (pr);
			if (pr->depth == exitdepth)
				return;		// all done
			break;

		case OP_STATE:
			ed = PROG_TO_EDICT(pr, pr->global_struct->self);
			ed->v.nextthink = pr->global_struct->time + 0.1;
			if (a->_float != ed->v.frame)
			{
				ed->v.frame = a->_float;
			}
			ed->v.think = b->function;
			break;

		default:
			PR_RunError (pr, "Bad opcode %d", st->op);
		}
	}
}
