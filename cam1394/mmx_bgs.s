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
# [引数]
# RGBA* lpDst     出力画像のアドレス（画像は変更される)
# const RGBA* lpSrc  入力画像のアドレス（画像は変更されない)
# const RGBA* lpBack 
#                 背景画像のアドレス（画像は変更されない）
# int  sz          画像の画素数(=幅*高)	2の倍数であること。
# RGBA threshold  閾値 
#                 alpha成分(下位 8bit)は無視。
# [戻値]
#                 対象物体の画素数。
# [機能]
# mmxを使用し 背景差分法による対象の抽出を行なう。
# 対象でない画素はRGB=(0,0,0)となる。 	
#
# 引数で示される閾値Tについて、RGB成分のすべてが
# (S-B)+(B-S)<T の条件を満たす画素を抽出する。
# ただし "-" "+" はそれぞれ飽和減算、飽和加算をあらわす。 
# これは、各画素に対して 
#  R,G,B の各成分の(*lpSrc -*lpBack) の絶対値がすべて
#  R,G,B の各閾値より小さい場合のみ
# *lpSrc=RGBA(0,0,0,0) を格納することと等価である。
#
# [更新履歴]
# Tue Jan 11 19:18:53 2000 By hiromasa yoshimoto デバッグ完了。
# Fri Jan 28 05:15:01 2000 By hiromasa yoshimoto 高速化
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
	por	maskRGB,%mm7	/* 2閾値 (2画素分) */

	shrl	$1,%ecx		/*2画素づつ処理する*/
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

	
/* ストールの回避バージョン*/
		
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
	por	maskRGB,%mm7	/* 2閾値 (2画素分) */

	shrl	$2,%ecx		/* 4画素づつ処理する */
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
	






