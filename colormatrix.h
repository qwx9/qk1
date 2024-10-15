/* cmkind values for optimized special-casing */
enum {
	CmIdent, /* identity (== 1) */
	CmBright, /* only brightness is changed (> 1) */
};

#define CM(v) ((v)*(1<<12))

extern s16int cm[4*4];
extern cvar_t v_saturation;
extern cvar_t v_contrast;
extern cvar_t v_brightness;
extern int cmkind;

void cmsetvblend(float blend[4]);
void cmprocess(s16int cm[4*4], void *in, void *out, int n);
void cminit(void);
