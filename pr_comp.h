// this file is shared by quake and qcc

typedef int	func_t;
typedef int	string_t;

typedef enum {
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_function,
	ev_pointer,
}etype_t;

enum {
	OFS_NULL,
	OFS_RETURN,
	OFS_PARM0 = 4, // leave 3 ofs for each parm to hold vectors
	OFS_PARM1 = 7,
	OFS_PARM2 = 10,
	OFS_PARM3 = 13,
	OFS_PARM4 = 16,
	OFS_PARM5 = 19,
	OFS_PARM6 = 22,
	OFS_PARM7 = 25,
	RESERVED_OFS = 28,

	PROG_VERSION = 6,

	MAX_PARMS = 8,

	DEF_SAVEGLOBAL = 1<<15,

	PR_LUMP_STATEMENTS = 0, // statement 0 is an error
	PR_LUMP_GLOBALDEFS,
	PR_LUMP_FIELDDEFS,
	PR_LUMP_FUNCTIONS, // function 0 is an empty one
	PR_LUMP_STRINGS, // first string is a null string
	PR_LUMP_GLOBALS,
	NUM_PR_LUMPS,
};

enum {
	OP_DONE,
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,

	OP_EQ_F,
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,

	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,

	OP_LE,
	OP_GE,
	OP_LT,
	OP_GT,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,
	OP_GOTO,
	OP_AND,
	OP_OR,

	OP_BITAND,
	OP_BITOR
};

typedef struct {
	u16int op;
	s16int a,b,c;
}dstatement_t;

typedef struct {
	// if DEF_SAVEGLOBGAL bit is set
	// the variable needs to be saved in savegames
	u16int type;
	u16int ofs;
	int s_name;
}ddef_t;

typedef struct {
	int first_statement; // negative numbers are builtins
	int parm_start;
	int locals; // total ints of parms + locals
	int profile; // runtime
	int s_name;
	int s_file; // source file defined in
	int numparms;
	byte parm_size[MAX_PARMS];
}dfunction_t;

typedef struct {
	int off;
	int num;
}dproglump_t;

typedef struct {
	int version;
	int crc; // check of header file
	dproglump_t lumps[NUM_PR_LUMPS];
	int entityfields;
}dprograms_t;
