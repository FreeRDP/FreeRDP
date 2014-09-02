; a entire function for converting YUV420p data to the RGB format (without any special upconverting)
; It's completely written in nasm-x86-assembly for intel processors supporting SSSE3 and higher.
; Restrictions are that output data has to be aligned to 16 byte (a question of REAL performance!)
; and the width of resolution must be divisable by four.
;
section .text
	global check_ssse3

check_ssse3:
	push ebx
	
	pushf
	pop eax
	or eax,1<<21
	push eax
	popf
	pushf
	pop eax
	test eax,1<<21
	jz check_ssse3_end
	
	and eax,~(1<<21)
	push eax
	popf
	
	
	mov eax,1
	mov ebx,0
	cpuid
	test edx,1<<25	;sse
	jz check_ssse3_end
	test edx,1<<26	;sse2
	jz check_ssse3_end
	test ecx,1<<0	;sse3
	jz check_ssse3_end
	test ecx,1<<9	;ssse3
	jz check_ssse3_end
	
	
	pop ebx
	mov eax,0
	ret
	
	
check_ssse3_end:
	pop ebx
	mov eax,1
	ret
	
	
;extern int freerdp_image_yuv420p_to_xrgb(unsigned char *pDstData,unsigned char **pSrcData,int nWidth,int nHeight,int istride0,int istride1)
	global freerdp_image_yuv420p_to_xrgb
freerdp_image_yuv420p_to_xrgb:
	push ebx
	push ebp
	
;check wether stack is aligned to 16 byte boundary
;
;	---current stack value---|-----x-----|----42 byte---|---16 byte aligned stack---
;	lets say 508		 2	     506	    464
;		 1FCH		 2H	     1FAH	    1D0H
;						    1F0H    1D0H
;				 |------1FCH&FH----|1FCH&^FH
;				 |1FCH&FH-AH |--AH-|---16 byte aligned stack------------
;				We've got only one problem: what if 1FCH&FH was smaller than AH?
;				We could either add something to sp (impossible) or subtract 10H-(AH-1FCH&FH) [%10H]
;				That's the same like (1FCH&FH-AH+10H)&FH and (1FCH+6H)&FH
	mov eax,esp
	add eax,6H
	and eax,1111B
	sub esp,eax
	
	mov ebp,esp
	
;"local variables"
	sub esp,318	;res 8 -8,res 8 -16,res 8 -24,U 8 -32,nWidth 2 -34,nHeight 2 -36,iStride0 2 -38,iStride1 2 -40,last_line 1 -41,res 1 -42,G 16 -58,B 16 -74,
	;R 16 -90,add:128 16 -106,sub:128 16 -122,mul:48 16 -138,mul:475 16 -154,mul:403 16 -170,mul:120 16 -186,VaddY 4 -190,VaddUV 4 -194,stack offset 8 -202,
	;cmp:255 16 -218,cmp:0 16 -234,shuflleR 16 -250,andG 16 -266,shuffleB 16 -280,shuffleY 16 -296,shuffleUV 16 -314,scanline 4 -318
	
	;pDstData:edi,
	
	mov [ebp-202],eax
	
;last_line: if the last (U,V doubled) line should be skipped, set to 1B

	mov edi,[ebp+eax+12]

	mov ecx,[ebp+eax+16]
	mov esi,[ecx]
	mov ebx,[ecx+4]
	mov [ebp-32],ebx
	mov ebx,[ecx+8]
	
	
	mov edx,[ebp+eax+20]
	mov [ebp-34],dx
	
	shr word [ebp-34],2
	
	mov [ebp-318],edx
	shl dword [ebp-318],2
	
	
	mov ecx,[ebp+eax+24]
	
	mov [ebp-41],cl
	and byte [ebp-41],1B
	
	inc cx
	shr cx,1
	mov [ebp-36],cx
	
	
	mov ecx,[ebp+eax+28]
	mov [ebp-38],cx
	
	shl cx,1
	sub cx,dx
	mov [ebp-190],ecx
	
	
	mov ecx,[ebp+eax+32]
	mov [ebp-40],cx
	
	
	shr dx,1
	sub cx,dx
	mov [ebp-194],ecx

	
	mov eax,[ebp-32]
	
	
;init masks
	mov ecx,00000080H
	mov [ebp-106],ecx
	mov [ebp-102],ecx
	mov [ebp-98],ecx
	mov [ebp-94],ecx

	mov ecx,00800080H
	mov [ebp-122],ecx
	mov [ebp-118],ecx
	mov [ebp-114],ecx
	mov [ebp-110],ecx
	
	mov ecx,00300030H
	mov [ebp-138],ecx
	mov [ebp-134],ecx
	mov [ebp-130],ecx
	mov [ebp-126],ecx
	
	mov ecx,01DB01DBH
	mov [ebp-154],ecx
	mov [ebp-150],ecx
	mov [ebp-146],ecx
	mov [ebp-142],ecx
	
	mov ecx,01930193H
	mov [ebp-170],ecx
	mov [ebp-166],ecx
	mov [ebp-162],ecx
	mov [ebp-158],ecx
	
	mov ecx,00780078H
	mov [ebp-186],ecx
	mov [ebp-182],ecx
	mov [ebp-178],ecx
	mov [ebp-174],ecx
	
	mov ecx,000FF0000H
	mov [ebp-218],ecx
	mov [ebp-214],ecx
	mov [ebp-210],ecx
	mov [ebp-206],ecx
	
	mov ecx,00000000H
	mov [ebp-234],ecx
	mov [ebp-230],ecx
	mov [ebp-226],ecx
	mov [ebp-222],ecx
	
;shuffle masks
	;00 xx 00 00  00 xx 00 00  00 xx 00 00  00 xx 00 00
	;00 rr gg bb  00 rr gg bb  00 rr gg bb  00 rr gg bb
	mov ecx,00FF0000H
	mov [ebp-250],ecx
	mov [ebp-246],ecx
	mov [ebp-242],ecx
	mov [ebp-238],ecx
	
	mov ecx,80800280H
	mov [ebp-266],ecx
	mov ecx,80800680H
	mov [ebp-262],ecx
	mov ecx,80800A80H
	mov [ebp-258],ecx
	mov ecx,80800E80H
	mov [ebp-254],ecx
	
	mov ecx,80808002H
	mov [ebp-282],ecx
	mov ecx,80808006H
	mov [ebp-278],ecx
	mov ecx,8080800AH
	mov [ebp-274],ecx
	mov ecx,8080800EH
	mov [ebp-270],ecx
	
	;dd cc bb aa
	;00 00 dd 00  00 00 cc 00  00 00 bb 00  00 00 aa 00
	mov ecx,80800080H
	mov [ebp-298],ecx
	mov ecx,80800180H
	mov [ebp-294],ecx
	mov ecx,80800280H
	mov [ebp-290],ecx
	mov ecx,80800380H
	mov [ebp-286],ecx
	
	;dd cc bb aa
	;00 dd 00 dd  00 cc 00 cc  00 bb 00 bb  00 aa 00 aa
	mov ecx,80008000H
	mov [ebp-314],ecx
	mov ecx,80018001H
	mov [ebp-310],ecx
	mov ecx,80028002H
	mov [ebp-306],ecx
	mov ecx,80038003H
	mov [ebp-302],ecx
	
	
	
freerdp_image_yuv420p_to_xrgb_hloop:
	dec word [ebp-36]
	js freerdp_image_yuv420p_to_xrgb_hloop_end
	jnz not_last_line
	
	shl byte [ebp-41],1
not_last_line:
	
	mov cx,[ebp-34]
freerdp_image_yuv420p_to_xrgb_wloop:
;main loop
;	C = Y;
;	D = U - 128;
;	E = V - 128;
;
;	R = clip(( 256 * C           + 403 * E + 128) >> 8);
;	G = clip(( 256 * C -  48 * D - 120 * E + 128) >> 8);
;	B = clip(( 256 * C + 475 * D           + 128) >> 8);

	test cx,1B
	jnz load_yuv_data
	
	
	;prepare U data
	movd xmm0,[eax]
	movdqa xmm5,[ebp-314]
	pshufb xmm0,xmm5	;but this is the omest instruction of all!!
	
	add eax,4
	
	movdqa xmm3,[ebp-122]
	psubsw xmm0,xmm3
	
	movdqa xmm2,xmm0
	
	movdqa xmm4,xmm0
	movdqa xmm7,[ebp-138]
	pmullw xmm0,xmm7
	pmulhw xmm4,xmm7
	
	movdqa xmm7,xmm0
	punpcklwd xmm0,xmm4	;what an awesome instruction!
	punpckhwd xmm7,xmm4
	movdqa xmm4,xmm7
	
	movdqa xmm6,[ebp-106]
	psubd xmm0,xmm6
	psubd xmm4,xmm6
	
	
	movdqa xmm1,xmm2
	movdqa xmm7,[ebp-154]
	pmullw xmm1,xmm7
	pmulhw xmm2,xmm7
	
	movdqa xmm7,xmm1
	punpcklwd xmm1,xmm2
	punpckhwd xmm7,xmm2
	
	paddd xmm1,xmm6
	paddd xmm7,xmm6
	
	movdqa [ebp-74],xmm7
	
	
	;prepare V data
	movd xmm2,[ebx]
	pshufb xmm2,xmm5
	
	add ebx,4
	
	psubsw xmm2,xmm3
	
	movdqa xmm5,xmm2
	
	movdqa xmm3,xmm2
	movdqa xmm7,[ebp-170]
	pmullw xmm2,xmm7
	pmulhw xmm3,xmm7
	
	movdqa xmm7,xmm2
	punpcklwd xmm2,xmm3
	punpckhwd xmm7,xmm3
	
	paddd xmm2,xmm6
	paddd xmm7,xmm6
	
	movdqa [ebp-90],xmm7
	
	
	movdqa xmm3,xmm5
	movdqa xmm7,[ebp-186]
	pmullw xmm3,xmm7
	pmulhw xmm5,xmm7
	
	movdqa xmm7,xmm3
	punpcklwd xmm3,xmm5
	punpckhwd xmm7,xmm5
	
	paddd xmm0,xmm3
	paddd xmm4,xmm7
	
	movdqa [ebp-58],xmm4
	
	jmp valid_yuv_data
		
load_yuv_data:
	movdqa xmm1,[ebp-74]
	movdqa xmm2,[ebp-90]
	movdqa xmm0,[ebp-58]
	
valid_yuv_data:
	
	
	;Y data processing
	movd xmm4,[esi]
	pshufb xmm4,[ebp-298]
	
	movdqa xmm5,xmm4
	movdqa xmm6,xmm4
	
	paddd xmm4,xmm2
	psubd xmm5,xmm0
	paddd xmm6,xmm1
	
	pslld xmm4,8
	pslld xmm5,8
	pslld xmm6,8
	
	movdqa xmm7,[ebp-234]
	pmaxsw xmm4,xmm7	;what an awesome instruction!
	pmaxsw xmm5,xmm7
	pmaxsw xmm6,xmm7
	
	movdqa xmm7,[ebp-218]
	pminsw xmm4,xmm7
	pminsw xmm5,xmm7
	pminsw xmm6,xmm7
	
	pand xmm4,[ebp-250]
	pshufb xmm5,[ebp-266]
	pshufb xmm6,[ebp-282]
	
	por xmm4,xmm5
	por xmm4,xmm6
	
	movdqa [edi],xmm4
	
	
	;Y data processing in secound line
	test byte [ebp-41],2
	jnz skip_last_line1
	
	mov dx,[ebp-38]
	and edx,0FFFFH
	movd xmm4,[esi+edx]
	pshufb xmm4,[ebp-298]
	
	
	movdqa xmm5,xmm4
	movdqa xmm6,xmm4
	
	paddd xmm4,xmm2
	psubd xmm5,xmm0
	paddd xmm6,xmm1
	
	pslld xmm4,8
	pslld xmm5,8
	pslld xmm6,8
	
	movdqa xmm7,[ebp-234]
	pmaxsw xmm4,xmm7	;what an awesome instruction!
	pmaxsw xmm5,xmm7
	pmaxsw xmm6,xmm7
	
	movdqa xmm7,[ebp-218]
	pminsw xmm4,xmm7
	pminsw xmm5,xmm7
	pminsw xmm6,xmm7
	
	pand xmm4,[ebp-250]
	pshufb xmm5,[ebp-266]
	pshufb xmm6,[ebp-282]
	
	por xmm4,xmm5
	por xmm4,xmm6
	
	mov edx,[ebp-318]
	movdqa [edi+edx],xmm4
	
skip_last_line1:
	add edi,16
	add esi,4
	
	dec cx
	jne freerdp_image_yuv420p_to_xrgb_wloop

freerdp_image_yuv420p_to_xrgb_wloop_end:
	mov edx,[ebp-318]
	add edi,edx
	
	mov edx,[ebp-190]
	add esi,edx
	
	mov edx,[ebp-194]
	add eax,edx
	add ebx,edx
	
	jmp freerdp_image_yuv420p_to_xrgb_hloop
	
freerdp_image_yuv420p_to_xrgb_hloop_end:

	mov eax,0
freerdp_image_yuv420p_to_xrgb_end:
	mov edx,[ebp-202]
	
	mov esp,ebp
	add esp,edx
	pop ebp
	pop ebx
	ret
