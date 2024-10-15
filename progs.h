#include "pr_comp.h"			// defs shared with qcc
#include "progdefs.h"			// generated by program cdefs

typedef struct pr_t pr_t;

typedef struct
{
	int				s;
	dfunction_t		*f;
} prstack_t;

#define	MAX_STACK_DEPTH		2048

typedef void (*builtin_t) (pr_t *pr);

#define	MAX_ENT_LEAFS	32
typedef struct edict_s
{
	bool free;
	link_t area;				// linked to a division node or leaf
	byte alpha;

	int num_leafs;
	int leafnums[MAX_ENT_LEAFS];

	entity_state_t baseline;

	float freetime; // sv.time when the object was freed
	entvars_t v; // C exported fields from progs
// other fields from progs come immediately after
} edict_t;
#define	EDICT_FROM_AREA(l) (edict_t*)((byte*)l - offsetof(edict_t, area))

struct pr_t {
	dfunction_t *functions;
	ddef_t *globaldefs;
	ddef_t *fielddefs;
	dstatement_t *statements;
	union {
		globalvars_t *global_struct;
		float *globals;
	};
	char *strings;
	int strings_size;
	int numfunctions;
	int numglobaldefs;
	int numfielddefs;
	int entityfields;
	int edict_size; // in bytes

	const builtin_t *builtins;
	int numbuiltins;
	int argc;
	bool trace;
	dfunction_t	*xfunction;
	int xstatement;

	prstack_t stack[MAX_STACK_DEPTH];
	int depth;

	// allocated/temp strings
	char **str;
	char *tempstr;
	int max_str;
	int num_str;
	int num_tempstr;

	edict_t *edicts;
	int num_edicts;
	int max_edicts;

	// krimzon parse client command
	struct {
		dfunction_t *func;
		int argc, argv[64];
		bool in_callback;
	}parse_cl_command;
};

typedef union eval_s
{
	string_t		string;
	float			_float;
	float			vector[3];
	func_t			function;
	int				_int;
	int				edict;
} eval_t;

//============================================================================

void PR_Init (void);

void PR_ExecuteProgram (pr_t *pr, func_t fnum);
pr_t *PR_LoadProgs (char *name);

char *PR_StrTmp(pr_t *pr);
int PR_CopyStrTmp(pr_t *pr, char *s);
int PR_SetStr(pr_t *pr, char *s);
char *PR_Str(pr_t *pr, int ofs);
char *PR_UglyValueString(pr_t *pr, etype_t, eval_t *);

void PR_Profile_f (cmd_t *c);

edict_t *ED_Alloc(pr_t *pr);
void ED_Free(edict_t *ed);

string_t ED_NewString (pr_t *pr, char *string);
// returns a copy of the string allocated from the server's string heap

void ED_Print (pr_t *pr, edict_t *ed);
char *ED_ParseEdict (pr_t *pr, char *data, edict_t *ent);

dfunction_t *ED_FindFunction (pr_t *pr, char *name);

void ED_ParseGlobals (pr_t *pr, char *data);
void ED_LoadFromFile (pr_t *pr, char *data);

edict_t *EDICT_NUM(pr_t *pr, int n);
int NUM_FOR_EDICT(pr_t *pr, edict_t *e);

#define	NEXT_EDICT(pr,e) ((edict_t *)( (byte *)e + pr->edict_size))

#define	EDICT_TO_PROG(pr,e) ((byte *)e - (byte *)pr->edicts)
#define PROG_TO_EDICT(pr,e) ((edict_t *)((byte *)pr->edicts + e))

//============================================================================

#define	G_FLOAT(pr,o) (pr->globals[o])
#define	G_INT(pr,o) (*(int *)&pr->globals[o])
#define	G_EDICT(pr,o) ((edict_t *)((byte *)pr->edicts+ *(int *)&pr->globals[o]))
#define G_EDICTNUM(pr,o) NUM_FOR_EDICT(pr, G_EDICT(pr,o))
#define	G_VECTOR(pr,o) (&pr->globals[o])
#define	G_STRING(pr,o) (PR_Str(pr, *(string_t *)&pr->globals[o]))
#define	G_FUNCTION(pr,o) (*(func_t *)&pr->globals[o])

#define	E_FLOAT(e,o) (((float*)&e->v)[o])
#define	E_INT(e,o) (*(int *)&((float*)&e->v)[o])
#define	E_VECTOR(e,o) (&((float*)&e->v)[o])
#define	E_STRING(pr,e,o) (PR_Str(pr,*(string_t *)&((float*)&e->v)[o]))

extern const int type_size[8];

#pragma varargck	argpos	PR_RunError	2
void PR_RunError (pr_t *pr, char *error, ...);

ddef_t *ED_FieldAtOfs (pr_t *pr, int ofs);
void ED_PrintEdicts(cmd_t *c);
void ED_PrintNum(pr_t *pr, int ent);

eval_t *GetEdictFieldValue(pr_t *pr, edict_t *ed, char *field);

char *PR_GlobalString(pr_t *pr, int ofs);
char *PR_GlobalStringNoContents(pr_t *pr, int ofs);
void PF_changeyaw(pr_t *pr);
void PR_InitSV(pr_t *pr);
