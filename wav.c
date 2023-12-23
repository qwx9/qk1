#include "quakedef.h"

int
wavinfo(byte *in, int len, wavinfo_t *info)
{
	int i, n, sz, fmt, loopsamples;
	byte *p;

	memset(info, 0, sizeof(*info));

	p = in;
	if(len < 36 || memcmp(p, "RIFF", 4) != 0){
		werrstr("not a RIFF");
		return -1;
	}
	p += 4; len -= 4;
	sz = le32(p); len -= 4;
	if(sz < 36 || sz > len){
		werrstr("trucated? sz=%d len=%d", sz, len);
		return -1;
	}
	if(memcmp(p, "WAVE", 4) != 0){
		werrstr("not a WAVE");
		return -1;
	}
	p += 4; sz -= 4;
	if(memcmp(p, "fmt ", 4) != 0){
		werrstr("no \"fmt \" subchunk");
		return -1;
	}
	p += 4; sz -= 4;
	n = le32(p); sz -= 4;

	fmt = le16(p); sz -= 2; n -= 2;
	if(fmt != 1){
		werrstr("not Microsoft PCM format: %d", fmt);
		return -1;
	}
	info->channels = le16(p); sz -= 2; n -= 2;
	info->rate = le32(p); sz -= 4; n -= 4;
	p += 4+2; sz -= 4+2; n -= 4+2; /* skip ByteRate + BlockAlign */
	info->width = le16(p); sz -= 2; n -= 2;
	if((info->width % 8) != 0){
		werrstr("invalid width: %d", info->width);
		return -1;
	}
	info->width /= 8;
	info->loopofs = -1;
	loopsamples = 0;
	p += n; sz -= n;

	for(; sz >= 8;){
		p += 4; sz -= 4;
		n = le32(p); sz -= 4;
		if(n > sz || n <= 0)
			break;
		if(n >= 4){
			if(memcmp(p-8, "cue ", 4) == 0){
				i = le32(p); sz -= 4; n -= 4; /* CuePoints - usually two of those */
				if(i >= 1 && n >= i*6*4){
					p += 5*4; sz -= 5*4; n -= 5*4; /* Name+Position+Chunk+ChunkStart+BlockStart */
					info->loopofs = le32(p); sz -= 4; n -= 4;
					/* FIXME(sigrid): check if this is needed and whether it works at all */
					if(i > 1){
						p += 5*4; sz -= 5*4; n -= 5*4; /* Name+Position+Chunk+ChunkStart+BlockStart */
						loopsamples = info->loopofs + le32(p); sz -= 4; n -= 4;
					}
				}
			}else if(memcmp(p-8, "data", 4) == 0 && info->dataofs == 0){
				info->samples = n / info->width;
				info->dataofs = p - in;
			}else if(memcmp(p-8, "LIST", 4) == 0 && info->loopofs >= 0 && loopsamples == 0){
				p += 4; sz -= 4; n -= 4; /* skip "adtl" */
				if(n >= 20 && memcmp(p, "ltxt", 4) == 0 && memcmp(p+16, "mark", 4) == 0){
					p += 12; sz -= 12; n -= 12;
					loopsamples = info->loopofs + le32(p); sz -= 4; n -= 4;
				}
			}
		}
		n = (n + 1) & ~1;
		p += n; sz -= n;
	}

	if(loopsamples > info->samples || info->loopofs > info->samples){
		werrstr("invalid loop: (%d,%d) > %d", info->loopofs, loopsamples, info->samples);
		return -1;
	}
	if(loopsamples > 0)
		info->samples = loopsamples;

	return 0;
}
