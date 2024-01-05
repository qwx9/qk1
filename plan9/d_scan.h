#define dospan_solid(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep)\
{\
	if(((t + tstep*spancount) >> 16) == (t >> 16)){\
		pbase += (t >> 16) * width;\
		do{\
			*pdest++ = pbase[s >> 16];\
			*pz++ = izi;\
			s += sstep;\
			izi += izistep;\
		}while(--spancount);\
	}else if(((s + sstep*spancount) >> 16) == (s >> 16)){\
		pbase += s >> 16;\
		do{\
			*pdest++ = pbase[(t >> 16) * width];\
			*pz++ = izi;\
			t += tstep;\
			izi += izistep;\
		}while(--spancount);\
	}else{\
		do{\
			*pdest++ = pbase[(s >> 16) + (t >> 16) * width];\
			*pz++ = izi;\
			s += sstep;\
			t += tstep;\
			izi += izistep;\
		}while(--spancount);\
	}\
}

#define dospan_solid_f1(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep, fog)\
{\
	if(((t + tstep*spancount) >> 16) == (t >> 16)){\
		pbase += (t >> 16) * width;\
		do{\
			*pz++ = izi;\
			izi += izistep;\
			*pdest++ = blendfog(pbase[s >> 16], *fog);\
			fogstep(*fog);\
			s += sstep;\
		}while(--spancount);\
	}else if(((s + sstep*spancount) >> 16) == (s >> 16)){\
		pbase += s >> 16;\
		do{\
			*pz++ = izi;\
			izi += izistep;\
			*pdest++ = blendfog(pbase[(t >> 16) * width], *fog);\
			fogstep(*fog);\
			t += tstep;\
		}while(--spancount);\
	}else{\
		do{\
			*pz++ = izi;\
			izi += izistep;\
			*pdest++ = blendfog(pbase[(s >> 16) + (t >> 16) * width], *fog);\
			fogstep(*fog);\
			s += sstep;\
			t += tstep;\
		}while(--spancount);\
	}\
}

#define dospan_blend(pdest, pbase, s, t, sstep, tstep, spancount, width, alpha, pz, izi, izistep)\
{\
	pixel_t pix;\
\
	if(((t + tstep*spancount) >> 16) == (t >> 16)){\
		pbase += (t >> 16) * width;\
		do{\
			pix = pbase[s >> 16];\
			s += sstep;\
			if(opaque(pix) && *pz <= izi)\
				*pdest = blendalpha(pix, *pdest, alpha);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else if(((s + sstep*spancount) >> 16) == (s >> 16)){\
		pbase += s >> 16;\
		do{\
			pix = pbase[(t >> 16) * width];\
			t += tstep;\
			if(opaque(pix) && *pz <= izi)\
				*pdest = blendalpha(pix, *pdest, alpha);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else{\
		do{\
			pix = pbase[(s >> 16) + (t >> 16) * width];\
			s += sstep;\
			t += tstep;\
			if(opaque(pix) && *pz <= izi)\
				*pdest = blendalpha(pix, *pdest, alpha);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}\
}

#define dospan_blend_f1(pdest, pbase, s, t, sstep, tstep, spancount, width, alpha, pz, izi, izistep, fog)\
{\
	pixel_t pix;\
\
	if(((t + tstep*spancount) >> 16) == (t >> 16)){\
		pbase += (t >> 16) * width;\
		do{\
			pix = pbase[s >> 16];\
			s += sstep;\
			if(opaque(pix) && *pz <= izi)\
				*pdest = blendalpha(0xff<<24 | blendfog(pix, *fog), *pdest, alpha);\
			fogstep(*fog);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else if(((s + sstep*spancount) >> 16) == (s >> 16)){\
		pbase += s >> 16;\
		do{\
			pix = pbase[(t >> 16) * width];\
			t += tstep;\
			if(opaque(pix) && *pz <= izi)\
				*pdest = blendalpha(0xff<<24 | blendfog(pix, *fog), *pdest, alpha);\
			fogstep(*fog);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else{\
		do{\
			pix = pbase[(s >> 16) + (t >> 16) * width];\
			s += sstep;\
			t += tstep;\
			if(opaque(pix) && *pz <= izi)\
				*pdest = blendalpha(0xff<<24 | blendfog(pix, *fog), *pdest, alpha);\
			fogstep(*fog);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}\
}

#define dospan_fence(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep)\
{\
	pixel_t pix;\
\
	if(((t + tstep*spancount) >> 16) == (t >> 16)){\
		pbase += (t >> 16) * width;\
		do{\
			pix = pbase[s >> 16];\
			s += sstep;\
			if(opaque(pix) && *pz <= izi){\
				*pdest = pix;\
				*pz = izi;\
			}\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else if(((s + sstep*spancount) >> 16) == (s >> 16)){\
		pbase += s >> 16;\
		do{\
			pix = pbase[(t >> 16) * width];\
			t += tstep;\
			if(opaque(pix) && *pz <= izi){\
				*pdest = pix;\
				*pz = izi;\
			}\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else{\
		do{\
			pix = pbase[(s >> 16) + (t >> 16) * width];\
			s += sstep;\
			t += tstep;\
			if(opaque(pix) && *pz <= izi){\
				*pdest = pix;\
				*pz = izi;\
			}\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}\
}

#define dospan_fence_f1(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep, fog)\
{\
	pixel_t pix;\
\
	if(((t + tstep*spancount) >> 16) == (t >> 16)){\
		pbase += (t >> 16) * width;\
		do{\
			pix = pbase[s >> 16];\
			s += sstep;\
			if(opaque(pix) && *pz <= izi){\
				*pdest = blendfog(pix, *fog);\
				*pz = izi;\
			}\
			fogstep(*fog);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else if(((s + sstep*spancount) >> 16) == (s >> 16)){\
		pbase += s >> 16;\
		do{\
			pix = pbase[(t >> 16) * width];\
			t += tstep;\
			if(opaque(pix) && *pz <= izi){\
				*pdest = blendfog(pix, *fog);\
				*pz = izi;\
			}\
			fogstep(*fog);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}else{\
		do{\
			pix = pbase[(s >> 16) + (t >> 16) * width];\
			s += sstep;\
			t += tstep;\
			if(opaque(pix) && *pz <= izi){\
				*pdest = blendfog(pix, *fog);\
				*pz = izi;\
			}\
			fogstep(*fog);\
			izi += izistep;\
			pdest++;\
			pz++;\
		}while(--spancount);\
	}\
}

static void
dospan_turb(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, byte alpha, uzint *pz, uzint izi, int izistep)
{
	int sturb, tturb;
	bool noblend;

	noblend = (r_drawflags & DRAW_BLEND) == 0;
	s &= (CYCLE<<16)-1;
	t &= (CYCLE<<16)-1;

	do{
		if(noblend || *pz <= izi){
			sturb = ((s + r_turb_turb[(t>>16)&(CYCLE-1)])>>16)&63;
			tturb = ((t + r_turb_turb[(s>>16)&(CYCLE-1)])>>16)&63;
			*pdest = blendalpha(*(pbase + (tturb<<6) + sturb), *pdest, alpha);
			if(noblend)
				*pz = izi;
		}
		s += sstep;
		t += tstep;
		izi += izistep;
		pdest++;
		pz++;
	}while(--spancount > 0);
}

static void
dospan_turb_f1(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, byte alpha, uzint *pz, uzint izi, int izistep, fog_t *fog)
{
	int sturb, tturb;
	bool noblend;

	noblend = (r_drawflags & DRAW_BLEND) == 0;
	s &= (CYCLE<<16)-1;
	t &= (CYCLE<<16)-1;

	do{
		if(noblend || *pz <= izi){
			sturb = ((s + r_turb_turb[(t>>16)&(CYCLE-1)])>>16)&63;
			tturb = ((t + r_turb_turb[(s>>16)&(CYCLE-1)])>>16)&63;
			*pdest = blendalpha(0xff<<24 | blendfog(*(pbase + (tturb<<6) + sturb), *fog), *pdest, alpha);
			if(noblend)
				*pz = izi;
		}
		s += sstep;
		t += tstep;
		izi += izistep;
		pdest++;
		pz++;
		fogstep(*fog);
	}while(--spancount > 0);
}
