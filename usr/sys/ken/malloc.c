#
/*
 */

/*
 * Structure of the coremap and swapmap
 * arrays. Consists of non-zero count
 * and base address of that many
 * contiguous units.
 * (The coremap unit is 64 bytes,
 * the swapmap unit is 512 bytes)
 * The addresses are increasing and
 * the list is terminated with the
 * first zero count.
 */
struct map
{
	char *m_size;
	char *m_addr;
};

/*
 * Allocate size units from the given
 * map. Return the base of the allocated
 * space.
 * Algorithm is first fit.
 */
malloc(mp, size)
struct map *mp;
{
	register int a;
	register struct map *bp;

	for (bp = mp; bp->m_size; bp++) {			//map을 증가시키면서 센터닐 전까지 동작
		if (bp->m_size >= size) {			//빈공간의 크기가 요청한 크기보다 크면
			a = bp->m_addr;
			bp->m_addr =+ size;			//빈공간의 주소 증가
			if ((bp->m_size =- size) == 0)		//빈공간의 사이즈와 요청한 사이즈가 같을 때
				do {
					bp++;
					(bp-1)->m_addr = bp->m_addr;		//다음 빙공간을 한개씩 앞으로 당김
				} while ((bp-1)->m_size = bp->m_size);		//센티널이 나올 때까지 반복
			return(a);						//할당 할 수 있는 메모리 주소 할당
		}
	}
	return(0);
}

/*
 * Free the previously allocated space aa
 * of size units into the specified map.
 * Sort aa into map and combine on
 * one or both ends if possible.
 */
mfree(mp, size, aa)
struct map *mp;
{
	register struct map *bp;
	register int t;
	register int a;

	a = aa;
	for (bp = mp; bp->m_addr<=a && bp->m_size!=0; bp++);
	if (bp>mp && (bp-1)->m_addr+(bp-1)->m_size == a) {
		(bp-1)->m_size =+ size;
		if (a+size == bp->m_addr) {
			(bp-1)->m_size =+ bp->m_size;
			while (bp->m_size) {
				bp++;
				(bp-1)->m_addr = bp->m_addr;
				(bp-1)->m_size = bp->m_size;
			}
		}
	} else {
		if (a+size == bp->m_addr && bp->m_size) {
			bp->m_addr =- size;
			bp->m_size =+ size;
		} else if (size) do {
			t = bp->m_addr;
			bp->m_addr = a;
			a = t;
			t = bp->m_size;
			bp->m_size = size;
			bp++;
		} while (size = t);
	}
}
