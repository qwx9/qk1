#include "quakedef.h"
#include "colormatrix.h"

extern int cmflags;

void
cmprocess(s16int cm[4*4], void *in_, void *out_, int n)
{
	pixel_t *in, *out;

	if(cmkind == CmIdent)
		return;

	in = in_;
	out = out_;

	if(cmkind == CmBright){
		for(; n > 0; in++, n--){
			s32int x[4] = {
				(*in>>0)&0xff,
				(*in>>8)&0xff,
				(*in>>16)&0xff,
				0xff,
			};
			s32int y[4] = {
				x[0]*cm[4*0+0]/CM(1),
				x[1]*cm[4*1+1]/CM(1),
				x[2]*cm[4*2+2]/CM(1),
				0xff,
			};
			u8int z[4] = {
				min(y[0], 255),
				min(y[1], 255),
				min(y[2], 255),
				0xff,
			};
			*out++ = z[0]<<0 | z[1]<<8 | z[2]<<16 | (pixel_t)0xff<<24;
		}
		return;
	}

	for(; n > 0; in++, n--){
		s32int x[4] = {
			(*in>>0)&0xff,
			(*in>>8)&0xff,
			(*in>>16)&0xff,
			0xff,
		};
		s32int y[4] = {
			(x[0]*cm[4*0+0] + x[1]*cm[4*0+1] + x[2]*cm[4*0+2] + 0xff*cm[4*0+3])/CM(1),
			(x[0]*cm[4*1+0] + x[1]*cm[4*1+1] + x[2]*cm[4*1+2] + 0xff*cm[4*1+3])/CM(1),
			(x[0]*cm[4*2+0] + x[1]*cm[4*2+1] + x[2]*cm[4*2+2] + 0xff*cm[4*2+3])/CM(1),
			0xff,
		};
		u32int z[4] = {
			clamp(y[0], 0, 255),
			clamp(y[1], 0, 255),
			clamp(y[2], 0, 255),
			0xff,
		};
		*out++ = z[0]<<0 | z[1]<<8 | z[2]<<16 | z[3]<<24;
	}
}
