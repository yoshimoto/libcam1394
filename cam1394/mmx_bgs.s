#/* 
#   Tue Jan 11 19:12:34 2000 By hiromasa yoshimoto 
#   Fri Jan 28 03:35:12 2000 By hiromasa yoshimoto
#*/
	.text
	.align	4
#/*
#
# void
# mmx_bgs(lpDst,lpSrc,lpBack,sz,threshold)
#
# [����]
# RGBA* lpDst     ���ϲ����Υ��ɥ쥹�ʲ������ѹ������)
# const RGBA* lpSrc  ���ϲ����Υ��ɥ쥹�ʲ������ѹ�����ʤ�)
# const RGBA* lpBack 
#                 �طʲ����Υ��ɥ쥹�ʲ������ѹ�����ʤ���
# int  sz          �����β��ǿ�(=��*��)	2���ܿ��Ǥ��뤳�ȡ�
# RGBA threshold  ���� 
#                 alpha��ʬ(���� 8bit)��̵�롣
# [����]
#                 �о�ʪ�Τβ��ǿ���
# [��ǽ]
# mmx����Ѥ� �طʺ�ʬˡ�ˤ���оݤ���Ф�Ԥʤ���
# �оݤǤʤ����Ǥ�RGB=(0,0,0)�Ȥʤ롣 	
#
# �����Ǽ����������T�ˤĤ��ơ�RGB��ʬ�Τ��٤Ƥ�
# (S-B)+(B-S)<T �ξ������������Ǥ���Ф��롣
# ������ "-" "+" �Ϥ��줾��˰�¸�����˰�²û��򤢤�魯�� 
# ����ϡ��Ʋ��Ǥ��Ф��� 
#  R,G,B �γ���ʬ��(*lpSrc -*lpBack) �������ͤ����٤�
#  R,G,B �γ����ͤ�꾮�������Τ�
# *lpSrc=RGBA(0,0,0,0) ���Ǽ���뤳�Ȥ������Ǥ��롣
#
# [��������]
# Tue Jan 11 19:18:53 2000 By hiromasa yoshimoto �ǥХå���λ��
# Fri Jan 28 05:15:01 2000 By hiromasa yoshimoto ��®��
#
#*/
	
.globl	mmx_bgs
	.type	mmx_bgs,@function

#mmx_bgs:
#	/* save regs	*/
	pushl	%ebp		
	movl	%esp,%ebp

#	pushl	%eax
	pushl	%ebx
#	pushl	%ecx
#	pushl	%edx

	movl	8(%ebp),%eax
	movl	12(%ebp),%edx
	movl	16(%ebp),%ebx
	movl	20(%ebp),%ecx
	movd	24(%ebp),%mm7
	
	movq	%mm7,%mm1
	psllq	$32,%mm7
	por	%mm1,%mm7
	por	maskRGB,%mm7	/* 2���� (2����ʬ) */

	shrl	$1,%ecx		/*2���ǤŤĽ�������*/
	pxor	%mm6,%mm6

	/*  1st param(src image's address)  => edx	*/
	/*  2nd param(back image's address) => ebx	*/
	/*  3rd param(size of image)	=> ecx		*/
	/*  4th param( threshold(R,G,B,0xff)   => mm7	*/
	/* const 0x00000000  => mm6			*/

mmx_bgs_loop:
	movq	(%edx),%mm0	/*	src  pixel -> S */
	movq	(%ebx),%mm1	/*	back pixel -> B */
	movq	%mm0,%mm2	/*	S -> S'		*/
	psubusb	%mm1,%mm0	/*	S - B -> S	*/
	psubusb %mm2,%mm1	/*	B - S' -> B	*/
	paddusb	%mm0,%mm1	/*	B + S  -> B	*/
	psubusb	%mm7,%mm1	/*	B - th -> B	*/
	pcmpgtd	%mm6,%mm1	/* if  S' > 0x00000000 
				/*		then 0xffffffff ->S' */
				/*		else 0x00000000 ->S' */
	pand	%mm2,%mm1	/*  S' & B -> B */
	movq	%mm1,(%eax)     /*  B -> src  */
	addl	$8,%ebx
	addl	$8,%edx
	addl	$8,%eax
	loop	mmx_bgs_loop
mmx_bgs_exit:
	/* clear mmx status */
#	emms
#	/* restore regs */

#	popl	%edx
#	popl	%ecx	
	popl	%ebx
#	popl	%eax
	
 	movl	%ebp,%esp
	popl	%ebp
	ret
	
maskRGB:
	.quad	0xff000000ff000000

	
/* ���ȡ���β���С������*/
		
.globl	mmx_bgs_ex
	.type	mmx_bgs_ex,@function

mmx_bgs:
mmx_bgs_ex:
	/* save regs	*/
	pushl	%ebp		
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	
	movl	8(%ebp),%eax
	movl	12(%ebp),%esi
	movl	16(%ebp),%edi
	movl	20(%ebp),%ecx
	movd	24(%ebp),%mm7
	
	movq	%mm7,%mm1
	psllq	$32,%mm7
	por	%mm1,%mm7
	por	maskRGB,%mm7	/* 2���� (2����ʬ) */

	shrl	$2,%ecx		/* 4���ǤŤĽ������� */
	pxor	%mm6,%mm6

	/*  1st param(src image's address)  => esi	*/
	/*  2nd param(back image's address) => edi	*/
	/*  3rd param(size of image)	=> ecx		*/
	/*  4th param( threshold(R,G,B,0xff)   => mm7	*/
	/* const 0x00000000  => mm6			*/

mmx_bgs_ex_loop:
	movq	(%esi),%mm0	/*	src  pixel -> S */
	movq	(%edi),%mm1	/*	back pixel -> B */
	addl	$8,%esi
	addl	$8,%edi
		movq	(%esi),%mm3	/*	src  pixel -> S */
		movq	(%edi),%mm4	/*	back pixel -> B */
	movq	%mm0,%mm2	/*	S -> S'		*/
		movq	%mm3,%mm5	/*	S -> S'		*/
	psubusb	%mm1,%mm0	/*	S - B -> S	*/
		psubusb	%mm4,%mm3	/*	S - B -> S	*/
	psubusb %mm2,%mm1	/*	B - S' -> B	*/
		psubusb %mm5,%mm4	/*	B - S' -> B	*/
	paddusb	%mm0,%mm1	/*	B + S  -> B	*/
		paddusb	%mm3,%mm4	/*	B + S  -> B	*/
	psubusb	%mm7,%mm1	/*	B - th -> B	*/
	addl	$8,%edi
		psubusb	%mm7,%mm4	/*	B - th -> B	*/
	pcmpgtd	%mm6,%mm1
	addl	$8,%esi
		pcmpgtd	%mm6,%mm4	
	pand	%mm2,%mm1	/*  S' & B -> B */
		pand	%mm5,%mm4	/*  S' & B -> B */
	movq	%mm1,(%eax)     /*  B -> src  */
	addl	$8,%eax
		movq	%mm4,(%eax)     /*  B -> src  */
		addl	$8,%eax
	loop	mmx_bgs_ex_loop
	
mmx_bgs_ex_exit:
	/* clear mmx status */
#	emms
#	/* restore regs */
	popl	%edi
	popl	%esi
	popl	%ebx
 	movl	%ebp,%esp
	popl	%ebp
	ret

				
/*  Fri Jan 28 05:23:11 2000 By hiromasa yoshimoto */
	






