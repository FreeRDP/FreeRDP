section .text
	global check_ssse3

check_ssse3:
	push rbx
	
	pushf
	pop rax
	or rax,1<<21
	push rax
	popf
	pushf
	pop rax
	test rax,1<<21
	jz check_ssse3_end
	
	and rax,~(1<<21)
	push rax
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
	
	
	pop rbx
	mov eax,0
	ret
	
	
check_ssse3_end:
	pop rbx
	mov eax,1
	ret
	
	
;extern int freerdp_image_yuv420p_to_xrgb(unsigned char *pDstData,unsigned char **pSrcData,int nWidth,int nHeight,int istride0,int istride1)
	global freerdp_image_yuv420p_to_xrgb
freerdp_image_yuv420p_to_xrgb:
	push rbx
	push rbp
	
;check wether stack is aligned to 16 byte boundary
	mov rax,rsp
	and rax,1111B
	mov r15,22
	sub r15b,al
	sub rsp,r15
	
	mov rbp,rsp
	
	xor r10,r10
	xor r11,r11
	xor r12,r12
	xor r13,r13
	xor r14,r14
	
	sub rsp,316	;pDstData 8,Y 8,U 8,V 8,nWidth 2,nHeight 2,iStride0 2,iStride1 2,last_column 1,last_line 1,G 16,B 16,R 16,add:128 16
	;sub:128 16,mul:48 16,mul:475 16,mul:403 16,mul:120 16,VaddY 2,VaddUV 2,res 12,cmp:255 16,cmp:0 16,shuflleR 16,andG 16,shuffleB 16,shuffleY 16,shuffleUV 16,scanline 2
	
;last_line: if the last (U,V doubled) line should be skipped, set to 1B
;last_column: if the last 4 columns should be skipped, set to 1B

	mov [rbp-8],rdi

	mov rax,[rsi]
	mov [rbp-16],rax
	mov rax,[rsi+8]
	mov [rbp-24],rax
	mov rax,[rsi+16]
	mov [rbp-32],rax
	
	mov [rbp-34],dx
	mov r13w,cx
	
	and r8,0FFFFH
	mov [rbp-38],r8w
	and r9,0FFFFH
	mov [rbp-40],r9w
	
	
	shl r8w,1
	sub r8w,dx
	mov r11w,r8w
	
	mov r10w,dx
	shr dx,1
	sub r9w,dx
	mov r12w,r9w
	
	
	mov r8w,[rbp-34]
	shr r8w,2
	shl r10w,2
	
	mov r9w,[rbp-38]
	
	;and al,11B
	;jz no_column_rest
	
	;inc word [rbp-34]
	
;no_column_rest:
	;mov [rbp-41],al
	
	
	
	mov r14b,r13b
	and r14b,1B
	;jz no_line_rest
	
	inc r13w

;no_line_rest:
	shr r13w,1
	
	
	
;init masks
	mov eax,00000080H
	mov [rbp-106],eax
	mov [rbp-102],eax
	mov [rbp-98],eax
	mov [rbp-94],eax

	mov eax,00800080H
	mov [rbp-122],eax
	mov [rbp-118],eax
	mov [rbp-114],eax
	mov [rbp-110],eax
	
	mov eax,00300030H
	mov [rbp-138],eax
	mov [rbp-134],eax
	mov [rbp-130],eax
	mov [rbp-126],eax
	
	mov eax,01DB01DBH
	mov [rbp-154],eax
	mov [rbp-150],eax
	mov [rbp-146],eax
	mov [rbp-142],eax
	
	mov eax,01930193H
	mov [rbp-170],eax
	mov [rbp-166],eax
	mov [rbp-162],eax
	mov [rbp-158],eax
	
	mov eax,00780078H
	mov [rbp-186],eax
	mov [rbp-182],eax
	mov [rbp-178],eax
	mov [rbp-174],eax
	
	mov eax,000FF0000H
	mov [rbp-218],eax
	mov [rbp-214],eax
	mov [rbp-210],eax
	mov [rbp-206],eax
	
	mov eax,00000000H
	mov [rbp-234],eax
	mov [rbp-230],eax
	mov [rbp-226],eax
	mov [rbp-222],eax
	
;shuffle masks
	;00 xx 00 00  00 xx 00 00  00 xx 00 00  00 xx 00 00
	;00 rr gg bb  00 rr gg bb  00 rr gg bb  00 rr gg bb
	mov eax,00FF0000H
	mov [rbp-250],eax
	mov [rbp-246],eax
	mov [rbp-242],eax
	mov [rbp-238],eax
	
	mov eax,80800280H
	mov [rbp-266],eax
	mov eax,80800680H
	mov [rbp-262],eax
	mov eax,80800A80H
	mov [rbp-258],eax
	mov eax,80800E80H
	mov [rbp-254],eax
	
	mov eax,80808002H
	mov [rbp-282],eax
	mov eax,80808006H
	mov [rbp-278],eax
	mov eax,8080800AH
	mov [rbp-274],eax
	mov eax,8080800EH
	mov [rbp-270],eax
	
	;dd cc bb aa
	;00 00 dd 00  00 00 cc 00  00 00 bb 00  00 00 aa 00
	mov eax,80800080H
	mov [rbp-298],eax
	mov eax,80800180H
	mov [rbp-294],eax
	mov eax,80800280H
	mov [rbp-290],eax
	mov eax,80800380H
	mov [rbp-286],eax
	
	;dd cc bb aa
	;00 dd 00 dd  00 cc 00 cc  00 bb 00 bb  00 aa 00 aa
	mov eax,80008000H
	mov [rbp-314],eax
	mov eax,80018001H
	mov [rbp-310],eax
	mov eax,80028002H
	mov [rbp-306],eax
	mov eax,80038003H
	mov [rbp-302],eax
	
	
	mov rsi,[rbp-16]
	mov rax,[rbp-24]
	mov rbx,[rbp-32]
	
	
freerdp_image_yuv420p_to_xrgb_hloop:
	dec r13w
	js freerdp_image_yuv420p_to_xrgb_hloop_end
	jnz not_last_line
	
	shl r14b,1
not_last_line:
	
	xor cx,cx
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
	movd xmm0,[rax]
	movdqa xmm5,[rbp-314]
	pshufb xmm0,xmm5
	
	add rax,4
	
	movdqa xmm3,[rbp-122]
	psubsw xmm0,xmm3
	
	movdqa xmm2,xmm0
	
	movdqa xmm4,xmm0
	movdqa xmm7,[rbp-138]
	pmullw xmm0,xmm7
	pmulhw xmm4,xmm7
	
	movdqa xmm7,xmm0
	punpcklwd xmm0,xmm4	;what an awesome instruction!
	punpckhwd xmm7,xmm4
	movdqa xmm4,xmm7
	
	movdqa xmm6,[rbp-106]
	psubd xmm0,xmm6
	psubd xmm4,xmm6
	
	
	movdqa xmm1,xmm2
	movdqa xmm7,[rbp-154]
	pmullw xmm1,xmm7
	pmulhw xmm2,xmm7
	
	movdqa xmm7,xmm1
	punpcklwd xmm1,xmm2
	punpckhwd xmm7,xmm2
	
	paddd xmm1,xmm6
	paddd xmm7,xmm6
	
	movdqa [rbp-74],xmm7
	
	
	;prepare V data
	movd xmm2,[rbx]
	pshufb xmm2,xmm5
	
	add rbx,4
	
	psubsw xmm2,xmm3
	
	movdqa xmm5,xmm2
	
	movdqa xmm3,xmm2
	movdqa xmm7,[rbp-170]
	pmullw xmm2,xmm7
	pmulhw xmm3,xmm7
	
	movdqa xmm7,xmm2
	punpcklwd xmm2,xmm3
	punpckhwd xmm7,xmm3
	
	paddd xmm2,xmm6
	paddd xmm7,xmm6
	
	movdqa [rbp-90],xmm7
	
	
	movdqa xmm3,xmm5
	movdqa xmm7,[rbp-186]
	pmullw xmm3,xmm7
	pmulhw xmm5,xmm7
	
	movdqa xmm7,xmm3
	punpcklwd xmm3,xmm5
	punpckhwd xmm7,xmm5
	
	paddd xmm0,xmm3
	paddd xmm4,xmm7
	
	movdqa [rbp-58],xmm4
	
	jmp valid_yuv_data
		
load_yuv_data:
	movdqa xmm1,[rbp-74]
	movdqa xmm2,[rbp-90]
	movdqa xmm0,[rbp-58]
	
valid_yuv_data:

	
	;Y data processing
	movd xmm4,[rsi]
	pshufb xmm4,[rbp-298]
	
	movdqa xmm5,xmm4
	movdqa xmm6,xmm4
	
	paddd xmm4,xmm2
	psubd xmm5,xmm0
	paddd xmm6,xmm1
	
	pslld xmm4,8
	pslld xmm5,8
	pslld xmm6,8
	
	movdqa xmm7,[rbp-234]
	pmaxsw xmm4,xmm7	;what an awesome instruction!
	pmaxsw xmm5,xmm7
	pmaxsw xmm6,xmm7
	
	movdqa xmm7,[rbp-218]
	pminsw xmm4,xmm7
	pminsw xmm5,xmm7
	pminsw xmm6,xmm7
	
	pand xmm4,[rbp-250]
	pshufb xmm5,[rbp-266]
	pshufb xmm6,[rbp-282]
	
	por xmm4,xmm5
	por xmm4,xmm6
	
	movdqa [rdi],xmm4
	
	
	;Y data processing in secound line
	test r14b,2
	jnz skip_last_line1
	
	movd xmm4,[rsi+r9]
	pshufb xmm4,[rbp-298]
	
	
	movdqa xmm5,xmm4
	movdqa xmm6,xmm4
	
	paddd xmm4,xmm2
	psubd xmm5,xmm0
	paddd xmm6,xmm1
	
	pslld xmm4,8
	pslld xmm5,8
	pslld xmm6,8
	
	movdqa xmm7,[rbp-234]
	pmaxsw xmm4,xmm7	;what an awesome instruction!
	pmaxsw xmm5,xmm7
	pmaxsw xmm6,xmm7
	
	movdqa xmm7,[rbp-218]
	pminsw xmm4,xmm7
	pminsw xmm5,xmm7
	pminsw xmm6,xmm7
	
	pand xmm4,[rbp-250]
	pshufb xmm5,[rbp-266]
	pshufb xmm6,[rbp-282]
	
	por xmm4,xmm5
	por xmm4,xmm6
	
	movdqa [rdi+r10],xmm4
	
skip_last_line1:
	add rdi,16
	add rsi,4
	
	inc cx
	cmp cx,r8w
	jne freerdp_image_yuv420p_to_xrgb_wloop

freerdp_image_yuv420p_to_xrgb_wloop_end:
	add rdi,r10
	
	add rsi,r11
	
	add rax,r12
	add rbx,r12
	;mov eax,r12d
	;jmp freerdp_image_yuv420p_to_xrgb_end

	jmp freerdp_image_yuv420p_to_xrgb_hloop
	
freerdp_image_yuv420p_to_xrgb_hloop_end:

	mov eax,0
freerdp_image_yuv420p_to_xrgb_end:
	mov rsp,rbp
	add rsp,r15
	pop rbp
	pop rbx
	ret