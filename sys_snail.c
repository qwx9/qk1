#ifdef SNAIL

#define _GNU_SOURCE
#include "quakedef.h"
#include <signal.h>
#include <sys/mman.h>
#include <sys/ucontext.h>

static bool snailing;
bool snailenabled;

static int
vidsize(void)
{
	return vid.width*vid.height*sizeof(pixel_t);
}

static void
wobble(int s, siginfo_t *info, void *ctx_)
{
	ucontext_t *ctx = ctx_;

	USED(info);
	if(!snailing)
		return;
	if(s == SIGSEGV){
		mprotect(vid.buffer, vidsize(), PROT_READ | PROT_WRITE);
		ctx->uc_mcontext.gregs[REG_EFL] |= 0x100;
	}else if(s == SIGTRAP){
		mprotect(vid.buffer, vidsize(), PROT_READ);
		ctx->uc_mcontext.gregs[REG_EFL] &= ~0x100;
		flipfb();
	}
}

void
sys_snail(bool enable)
{
	if(!snailenabled || enable == snailing)
		return;

	if(enable){
		memset(vid.buffer, 0, vidsize());
		flipfb();
	}
	if(mprotect(vid.buffer, vidsize(), PROT_READ | (enable ? 0 : PROT_WRITE)) < 0){
		perror("mprotect");
		return;
	}
	snailing = enable;
}

void
initsnail(void)
{
	struct sigaction act = {0};
	act.sa_sigaction = wobble;
	act.sa_flags = SA_SIGINFO;
	if(sigaction(SIGSEGV, &act, nil) < 0)
		perror("sigsegv");
	else if(sigaction(SIGTRAP, &act, nil) < 0)
		perror("sigtrap");
}

#else

#include "quakedef.h"

bool snailenabled;

void
sys_snail(bool enable)
{
	USED(enable);
}

void
initsnail(void)
{
	snailenabled = false;
}

#endif
