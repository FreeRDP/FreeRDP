; function for converting YUV420p data to the RGB format (but without any special upconverting)
; It's completely written in nasm-x86-assembly for intel processors supporting SSSE3 and higher.
; The target scanline (6th parameter) must be a multiple of 16.
; iStride[0] must be (target scanline) / 4 or bigger and iStride[1] the next multiple of four
; of the half of iStride[0] or bigger
;
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
	
	
;extern int freerdp_image_yuv420p_to_xrgb(unsigned char *pDstData,unsigned char **pSrcData,int nWidth,int nHeight,int *istride,int scanline)
	global freerdp_image_yuv420p_to_xrgb
freerdp_image_yuv420p_to_xrgb:
	push rbx
	push rbp
	
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
	mov r15,rsp
	add r15,6H
	and r15,1111B
	sub rsp,r15
	
	mov rbp,rsp
	
	xor r10,r10
	xor r11,r11
	xor r12,r12
	xor r13,r13
	xor r14,r14
	
;"local variables"
	sub rsp,338	;pDstData 8 -8,Y 8 -16,U 8 -24,V 8 -32,nWidth 2 -34,nHeight 2 -36,iStride0 2 -38,iStride1 2 -40,last_line 1 -41,last_column 1 -42,
	;G 16 -58,B 16 -74,R 16 -90,add:128 16 -106,sub:128 16 -122,mul:48 16 -138,mul:475 16 -154,mul:403 16 -170,mul:120 16 -186,VaddY 2 -188,VaddUV 2 -190,
	;res 12 -202,cmp:255 16 -218,cmp:0 16 -234,shuflleR 16 -250,andG 16 -266,shuffleB 16 -280,shuffleY 16 -296,shuffleUV 16 -314,andRemainingColumns 16 -330,
	;VddDst 8 -338
	
;last_line: if the last (U,V doubled) line should be skipped, set to 10B
;last_column: if it's the last column in a line, set to 10B (for handling line-endings not multiple by four)

	mov [rbp-8],rdi

	mov rax,[rsi]
	mov [rbp-16],rax
	mov rax,[rsi+8]
	mov [rbp-24],rax
	mov rax,[rsi+16]
	mov [rbp-32],rax
	
	mov [rbp-34],dx
	mov r13w,cx
	
	mov r10w,r9w
	and r10,0FFFFH
	
	
	mov ecx,[r8]
	mov [rbp-38],ecx
	mov r12d,[r8+4]
	mov [rbp-40],r12w
	
	
	mov [rbp-42],dl
	and byte [rbp-42],11B

	
	mov [rbp-338],r10
	shr word [rbp-338],1
	shl cx,1
	
	mov r8w,[rbp-34]
	add r8w,3
	and r8w, 0FFFCH
	
	sub [rbp-338],r8w
	sub cx,r8w
	
	shr r8w,1
	
	mov dx,r8w
	add dx,2
	and dx,0FFFCH
	sub r12w,dx
	
	shl dword [rbp-338],2
	mov r11w,cx
	
	shr r8w,1
	
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
	
;remaining columns and mask
	cmp byte [rbp-42],0
	je freerdp_image_yuv420p_to_xrgb_no_columns_remain

	mov dl,[rbp-42]
	xor ebx,ebx
	xor ecx,ecx
	xor esi,esi

	mov eax,0FFFFFFFFH
	cmp dl,1H
	je freerdp_image_yuv420p_to_xrgb_write_columns_remain
	
	mov ebx,0FFFFFFFFH
	cmp dl,2H
	je freerdp_image_yuv420p_to_xrgb_write_columns_remain
	
	mov ecx,0FFFFFFFFH
	
freerdp_image_yuv420p_to_xrgb_write_columns_remain:
	mov [rbp-330],eax
	mov [rbp-326],ebx
	mov [rbp-322],ecx
	mov [rbp-318],esi
	mov byte [rbp-42],1
	
freerdp_image_yuv420p_to_xrgb_no_columns_remain:
	
	
	mov rsi,[rbp-16]
	mov rax,[rbp-24]
	mov rbx,[rbp-32]
	
	;jmp freerdp_image_yuv420p_to_xrgb_end
	
freerdp_image_yuv420p_to_xrgb_hloop:
	dec r13w
	js freerdp_image_yuv420p_to_xrgb_hloop_end
	jnz not_last_line
	
	shl r14b,1
not_last_line:
	
	xor cx,cx
freerdp_image_yuv420p_to_xrgb_wloop:
; Well, in the end it should look like this:
;	C = Y;
;	D = U - 128;
;	E = V - 128;
;
;	R = clip(( 256 * C           + 403 * E + 128) >> 8);
;	G = clip(( 256 * C -  48 * D - 120 * E + 128) >> 8);
;	B = clip(( 256 * C + 475 * D           + 128) >> 8);

	test cx,1B
	jnz freerdp_image_yuv420p_to_xrgb_load_yuv_data
	
	
; Y-, U- and V-data is stored in different arrays.
; We start with processing U-data.

; at first we fetch four U-values from its array and shuffle them like this:
;	0d0d 0c0c 0b0b 0a0a
; we've done two things: converting the values to signed words and duplicating
; each value, because always two pixel "share" the same U- (and V-) data
	movd xmm0,[rax]
	movdqa xmm5,[rbp-314]
	pshufb xmm0,xmm5	;but this is the awesomest instruction of all!!
	
	add rax,4
	
; then we subtract 128 from each value, so we get D
	movdqa xmm3,[rbp-122]
	psubsw xmm0,xmm3
	
; we need to do two things with our D, so let's store it for later use
	movdqa xmm2,xmm0
	
; now we can multiply our D with 48 and unpack it to xmm4:xmm0
; this is what we need to get G data later on
	movdqa xmm4,xmm0
	movdqa xmm7,[rbp-138]
	pmullw xmm0,xmm7
	pmulhw xmm4,xmm7
	
	movdqa xmm7,xmm0
	punpcklwd xmm0,xmm4	;what an awesome instruction!
	punpckhwd xmm7,xmm4
	movdqa xmm4,xmm7
	
; to complete this step, add (?) 128 to each value (rounding ?!)
; yeah, add. in the end this will be subtracted from something,
; because it's part of G: 256*C - (48*D + 120*E - 128), 48*D-128 !
; by the way, our values have become signed dwords during multiplication!
	movdqa xmm6,[rbp-106]
	psubd xmm0,xmm6
	psubd xmm4,xmm6
	
	
; to get B data, we need to prepare a secound value, D*475+128
	movdqa xmm1,xmm2
	movdqa xmm7,[rbp-154]
	pmullw xmm1,xmm7
	pmulhw xmm2,xmm7
	
	movdqa xmm7,xmm1
	punpcklwd xmm1,xmm2
	punpckhwd xmm7,xmm2
	
	paddd xmm1,xmm6
	paddd xmm7,xmm6
	
; so we got something like this: xmm7:xmm1
; this pair contains values for 16 pixel:
; aabbccdd
; aabbccdd, but we can only work on four pixel at once, so we need to save upper values
	movdqa [rbp-74],xmm7
	
	
; Now we've prepared U-data. Preparing V-data is actually the same, just with other coefficients.
	movd xmm2,[rbx]
	pshufb xmm2,xmm5
	
	add rbx,4
	
	psubsw xmm2,xmm3
	
	movdqa xmm5,xmm2
	
; this is also known as E*403+128, we need it to convert R data
	movdqa xmm3,xmm2
	movdqa xmm7,[rbp-170]
	pmullw xmm2,xmm7
	pmulhw xmm3,xmm7
	
	movdqa xmm7,xmm2
	punpcklwd xmm2,xmm3
	punpckhwd xmm7,xmm3
	
	paddd xmm2,xmm6
	paddd xmm7,xmm6
	
; and preserve upper four values for future ...
	movdqa [rbp-90],xmm7
	
	
; doing this step: E*120
	movdqa xmm3,xmm5
	movdqa xmm7,[rbp-186]
	pmullw xmm3,xmm7
	pmulhw xmm5,xmm7
	
	movdqa xmm7,xmm3
	punpcklwd xmm3,xmm5
	punpckhwd xmm7,xmm5
	
; now we complete what we've begun above:
; (48*D-128) + (120*E) = (48*D +120*E -128)
	paddd xmm0,xmm3
	paddd xmm4,xmm7
	
; and store to memory !
	movdqa [rbp-58],xmm4
	
; real assembly programmers do not only produce best results between 0 and 5 o'clock,
; but are also kangaroos!
	jmp freerdp_image_yuv420p_to_xrgb_valid_yuv_data
	
freerdp_image_yuv420p_to_xrgb_load_yuv_data:
; maybe you've wondered about the conditional jump to this label above ?
; Well, we prepared UV data for eight pixel in each line, but can only process four
; per loop. So we need to load the upper four pixel data from memory each secound loop!
	movdqa xmm1,[rbp-74]
	movdqa xmm2,[rbp-90]
	movdqa xmm0,[rbp-58]
	
freerdp_image_yuv420p_to_xrgb_valid_yuv_data:

	inc cx
	cmp cx,r8w
	jne freerdp_image_yuv420p_to_xrgb_not_last_columns
	
	shl byte [rbp-42],1
	
	
freerdp_image_yuv420p_to_xrgb_not_last_columns:
	
; We didn't produce any output yet, so let's do so!
; Ok, fetch four pixel from the Y-data array and shuffle them like this:
; 00d0 00c0 00b0 00a0, to get signed dwords and multiply by 256
	movd xmm4,[rsi]
	pshufb xmm4,[rbp-298]
	
	movdqa xmm5,xmm4
	movdqa xmm6,xmm4
	
; no we can perform the "real" conversion itself and produce output!
	paddd xmm4,xmm2
	psubd xmm5,xmm0
	paddd xmm6,xmm1
	
; in the end, we only need bytes for RGB values.
; So, what do we do? right! shifting left makes values bigger and thats always good.
; before we had dwords of data, and by shifting left and treating the result
; as packed words, we get not only signed words, but do also divide by 256
; imagine, data is now ordered this way: ddx0 ccx0 bbx0 aax0, and x is the least
; significant byte, that we don't need anymore, because we've done some rounding
	pslld xmm4,8
	pslld xmm5,8
	pslld xmm6,8
	
; one thing we still have to face is the clip() function ...
; we have still signed words, and there are those min/max instructions in SSE2 ...
; the max instruction takes always the bigger of the two operands and stores it in the first one,
; and it operates with signs !
; if we feed it with our values and zeros, it takes the zeros if our values are smaller than
; zero and otherwise our values
	movdqa xmm7,[rbp-234]
	pmaxsw xmm4,xmm7	;what an awesome instruction!
	pmaxsw xmm5,xmm7
	pmaxsw xmm6,xmm7
	
; the same thing just completely different can be used to limit our values to 255,
; but now using the min instruction and 255s
	movdqa xmm7,[rbp-218]
	pminsw xmm4,xmm7
	pminsw xmm5,xmm7
	pminsw xmm6,xmm7
	
; Now we got our bytes.
; the moment has come to assemble the three channels R,G and B to the xrgb dwords
; on Red channel we just have to and each futural dword with 00FF0000H
	pand xmm4,[rbp-250]
; on Green channel we have to shuffle somehow, so we get something like this:
; 00d0 00c0 00b0 00a0
	pshufb xmm5,[rbp-266]
; and on Blue channel that one:
; 000d 000c 000b 000a
	pshufb xmm6,[rbp-282]
	
; and at last we or it together and get this one:
; xrgb xrgb xrgb xrgb
	por xmm4,xmm5
	por xmm4,xmm6
	
; Only thing to do know is writing data to memory, but this gets a bit more
; complicated if the width is not a multiple of four and it is the last column in line.
; but otherwise just play the kangaroo
	test byte [rbp-42],2
	je freerdp_image_yuv420p_to_xrgb_column_process_complete
	
; let's say, we need to only convert six pixel in width
; Ok, the first 4 pixel will be converted just like every 4 pixel else, but
; if it's the last loop in line, [rbp-42] is shifted left by one (curious? have a look above),
; and we land here. Through initialisation a mask was prepared. In this case it looks like
; 0000FFFFH 0000FFFFH 0000FFFFH 0000FFFFH
	movdqa xmm6,[rbp-330]
; we and our output data with this mask to get only the valid pixel
	pand xmm4,xmm6
; then we fetch memory from the destination array ...
	movdqu xmm5,[rdi]
; ... and and it with the inverse mask. We get only those pixel, which should not be updated
	pandn xmm6,xmm5
; we only have to or the two values together and write it back to the destination array,
; and only the pixel that should be updated really get changed.
	por xmm4,xmm6
	
freerdp_image_yuv420p_to_xrgb_column_process_complete:
	movdqu [rdi],xmm4
	
	
; Because UV data is the same for two lines, we can process the secound line just here,
; in the same loop. Only thing we need to do is to add some offsets to the Y- and destination
; pointer. These offsets are iStride[0] and the target scanline.
; But if we don't need to process the secound line, like if we are in the last line of processing nine lines,
; we just skip all this.
	test r14b,2
	jnz freerdp_yuv420p_to_xrgb_skip_last_line
	
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
	
	test byte [rbp-42],2
	je freerdp_image_yuv420p_to_xrgb_column_process_complete2
	
	movdqa xmm6,[rbp-330]
	pand xmm4,xmm6
	movdqu xmm5,[rdi+r10]
	pandn xmm6,xmm5
	por xmm4,xmm6
	
; only thing is, we should shift [rbp-42] back here, because we have processed the last column,
; and this "special condition" can be released
	shr byte [rbp-42],1
	
freerdp_image_yuv420p_to_xrgb_column_process_complete2:
	movdqu [rdi+r10],xmm4
	
	
freerdp_yuv420p_to_xrgb_skip_last_line:
; after all we have to increase the destination- and Y-data pointer by four pixel
	add rdi,16
	add rsi,4
	
	cmp cx,r8w
	jne freerdp_image_yuv420p_to_xrgb_wloop

freerdp_image_yuv420p_to_xrgb_wloop_end:
; after each line we have to add the scanline to the destination pointer, because
; we are processing two lines at once, but only increasing the destination pointer
; in the first line. Well, we only have one pointer, so it's the easiest way to access
; the secound line with the one pointer and an offset (scanline)
; if we're not converting the full width of the scanline, like only 64 pixel, but the
; output buffer was "designed" for 1920p HD, we have to add the remaining length for each line,
; to get into the next line.
	add rdi,[rbp-338]
	
; same thing has to be done for Y-data, but with iStride[0] instead of the target scanline
	add rsi,r11
	
; and again for UV data, but here it's enough to add the remaining length, because
; UV data is the same for two lines and there exists only one "UV line" on two "real lines"
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