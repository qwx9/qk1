#define N 3
static void
DrawSurfaceBlock(pixel_t *prowdest, pixel_t *psource, pixel_t *sourcemax, int deststep, int tstep, int stepback, unsigned *lp[N], unsigned lw, int nb)
{
	int b, v, i, j;
	int lightstep[N], light[N], lightleft[N], lightright[N];
	int lightleftstep[N], lightrightstep[N];

	for(v = 0; v < nb; v++){
		for(j = 0; j < N; j++){
			lightleft[j] = lp[j][0];
			lightright[j] = lp[j][1];
			lp[j] += lw;
			lightleftstep[j] = (lp[j][0] - lightleft[j]) >> (4-mip);
			lightrightstep[j] = (lp[j][1] - lightright[j]) >> (4-mip);
		}

		for(i = 0; i < 16>>mip; i++){
			for(j = 0; j < N; j++){
				lightstep[j] = (lightleft[j] - lightright[j]) >> (4-mip);
				light[j] = lightright[j];
			}

			for(b = (16>>mip)-1; b >= 0; b--){
				prowdest[b] = addlight(psource[b], light[0], light[1], light[2]);
				for(j = 0; j < N; j++)
					light[j] += lightstep[j];
			}

			psource += tstep;
			prowdest += deststep;

			for(j = 0; j < N; j++){
				lightright[j] += lightrightstep[j];
				lightleft[j] += lightleftstep[j];
			}
		}

		if(psource >= sourcemax)
			psource -= stepback;
	}
}
#undef N
