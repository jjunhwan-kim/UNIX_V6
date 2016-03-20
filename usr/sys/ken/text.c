#
/*
 */

#include "../param.h"
#include "../systm.h"
#include "../user.h"
#include "../proc.h"
#include "../text.h"
#include "../inode.h"

/*
 * Swap out process p.
 * The ff flag causes its core to be freed--
 * it may be off when called to create an image for a
 * child process in newproc.
 * Os is the old size of the data area of the process,
 * and is supplied during core expansion swaps.
 *
 * panic: out of swap space
 * panic: swap error -- IO error
 */
xswap(p, ff, os)
int *p;
{
	register *rp, a;

	rp = p;
	if(os == 0)
		os = rp->p_size;		//os인자가 0이면 프로세스의 p_size 사용
	a = malloc(swapmap, (rp->p_size+7)/8);	//스와프 영역에 확보
	if(a == NULL)
		panic("out of swap space");
	xccdec(rp->p_textp);			//텍스트 세그먼트 사용했으면 텍스트 세그먼트 영역 메모리 해제
	rp->p_flag =| SLOCK;
	if(swap(a, rp->p_addr, os, 0))		//스와프 아웃
		panic("swap error");
	if(ff)
		mfree(coremap, os, rp->p_addr);	//메모리 영역 해제
	rp->p_addr = a;				//p_addr스와프 영역 주소 저장
	rp->p_flag =& ~(SLOAD|SLOCK);		//SLOAD, SLOCK(스와핑 처리 중을 나타내는 플래그) 해제
	rp->p_time = 0;				//스와프 영역에 머무른 시간 초기화
	if(runout) {				//run out 플래그 초기화
		runout = 0;
		wakeup(&runout);
	}
}

/*
 * relinquish use of the shared text segment
 * of a process.
 */
xfree()
{
	register *xp, *ip;

	if((xp=u.u_procp->p_textp) != NULL) {
		u.u_procp->p_textp = NULL;
		xccdec(xp);
		if(--xp->x_count == 0) {
			ip = xp->x_iptr;
			if((ip->i_mode&ISVTX) == 0) {
				xp->x_iptr = NULL;
				mfree(swapmap, (xp->x_size+7)/8, xp->x_daddr);
				ip->i_flag =& ~ITEXT;
				iput(ip);
			}
		}
	}
}

/*
 * Attach to a shared text segment.
 * If there is no shared text, just return.
 * If there is, hook up to it:
 * if it is not currently being used, it has to be read
 * in from the inode (ip) and established in the swap space.
 * If it is being used, but is not currently in core,
 * a swap has to be done to get it back.
 * The full coroutine glory has to be invoked--
 * see slp.c-- because if the calling process
 * is misplaced in core the text image might not fit.
 * Quite possibly the code after "out:" could check to
 * see if the text does fit and simply swap it in.
 *
 * panic: out of swap space
 */
xalloc(ip)
int *ip;
{
	register struct text *xp;
	register *rp, ts;

	if(u.u_arg[1] == 0)
		return;
	rp = NULL;
	for(xp = &text[0]; xp < &text[NTEXT]; xp++)
		if(xp->x_iptr == NULL) {
			if(rp == NULL)
				rp = xp;			//빈 text[]엔트리가 있으면 그곳에 삽입
		} else
			if(xp->x_iptr == ip) {			//텍스트 엔트리에 inode 가 일치하는 텍스트 구조체가 있으면
				xp->x_count++;			//구조체의 count 증가시키고
				u.u_procp->p_textp = xp;	//구조체를 실행중인 프로세스에 할당
				goto out;
			}
	if((xp=rp) == NULL)					//텍스트 엔트리에 빈 엔트리 없고, inode가 일치하는 엔트리도 없을 때
		panic("out of text");
	xp->x_count = 1;
	xp->x_ccount = 0;
	xp->x_iptr = ip;
	ts = ((u.u_arg[1]+63)>>6) & 01777;			//텍스트 크기(??)
	xp->x_size = ts;
	if((xp->x_daddr = malloc(swapmap, (ts+7)/8)) == NULL)	//텍스트 크기 만큼스와프 영역 확보
		panic("out of swap space");
	expand(USIZE+ts);					//(??)
	estabur(0, ts, 0, 0);					//(??)
	u.u_count = u.u_arg[1];					//사용자 가상 어드레스 0x0에 읽음
	u.u_offset[1] = 020;					//실행 프로그램의 헤더를 건너 뜀
	u.u_base = 0;
	readi(ip);
	rp = u.u_procp;
	rp->p_flag =| SLOCK;
	swap(xp->x_daddr, rp->p_addr+USIZE, ts, 0);		//실행중인 프로세스의 paddr_USIZE에 존재하는 텍스트를 스와프 아웃
	rp->p_flag =& ~SLOCK;
	rp->p_textp = xp;
	rp = ip;
	rp->i_flag =| ITEXT;
	rp->i_count++;						//inode를 참조하고있는 프로세스의 수 증가
	expand(USIZE);						//(??)

out:
	if(xp->x_ccount == 0) {					//(??)
		savu(u.u_rsav);
		savu(u.u_ssav);
		xswap(u.u_procp, 1, 0);				//(??)
		u.u_procp->p_flag =| SSWAP;
		swtch();					//(??)
		/* no return */
	}
	xp->x_ccount++;
}

/*
 * Decrement the in-core usage count of a shared text segment.
 * When it drops to zero, free the core space.
 */
xccdec(xp)
int *xp;
{
	register *rp;

	if((rp=xp)!=NULL && rp->x_ccount!=0)			//프로세스가 텍스트 세그먼트를 참조하고 있으면
		if(--rp->x_ccount == 0)				//참조 수를 감소시키고 참조 수가 0이면 메모리 해제
			mfree(coremap, rp->x_size, rp->x_caddr);
}
