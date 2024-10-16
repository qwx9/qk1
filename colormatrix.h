/* cmkind values for optimized special-casing */
#define CmIdent 0 /* identity (== 1) */
#define CmBright 1 /* only brightness is changed (> 1) */

#define CM(v) ((v)*(1<<12))

#ifdef QUAKE_GAME
extern s16int cm[4*4];
extern cvar_t v_saturation;
extern cvar_t v_contrast;
extern cvar_t v_brightness;
extern int cmkind;

void cmsetvblend(float blend[4]);
void cmprocess(s16int cm[4*4], pixel_t *p, int n);
void cminit(void);
#endif
