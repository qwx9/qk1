extern cvar_t v_scale;

void V_Init(void);
void V_RenderView(void);
void V_ParseDamage (void);
float V_CalcRoll(vec3_t angles, vec3_t velocity);
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);
void V_SetContentsColor (int contents);
void V_ApplyShifts(void);
void V_NewMap(void);
