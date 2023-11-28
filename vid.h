// vid.h -- video driver defs

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

typedef u32int pixel_t;

typedef struct vrect_s
{
	int				x,y,width,height;
	struct vrect_s	*pnext;
} vrect_t;

typedef struct
{
	pixel_t			*buffer;		// invisible buffer
	pixel_t			*colormap;		// 256 * VID_GRADES size
	int				fullbright;		// index of first fullbright color
	unsigned		rowbytes;	// may be > width if displayed in a window
	int				width;
	int				height;
	float			aspect;		// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	pixel_t			*conbuffer;
	int				conrowbytes;
	unsigned		conwidth;
	unsigned		conheight;
	int				maxwarpwidth;
	int				maxwarpheight;
} viddef_t;

extern	viddef_t	vid;				// global video state
