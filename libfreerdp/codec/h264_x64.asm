;R=(256*Y+403*(V-128)+128)/265			=(256*Y+403*V-51456)/256
;G=(256*Y-48*(U-128)-120*(V-128)+128)/256	=(256*Y-48*U-120*V+21632)/256
;B=(256*Y+475*(U-128)+128)/256			=(256*Y+475*U-60672)/256

section .text
	;global YUV_to_RGB_asm
YUV_to_RGB_asm:
	shl rdi,8
	
	mov eax,edx
	imul eax,403
	add eax,edi
	sub eax,51456
	
	jae YUV_to_RGB_asm1
	mov eax,0
	jmp YUV_to_RGB_asm11

YUV_to_RGB_asm1:
	cmp eax, 0xFFFF
	jbe YUV_to_RGB_asm11
	mov eax,0xFF00
	
YUV_to_RGB_asm11:
	and eax,0xFF00
	shl eax,8
	
	mov ebx,esi
	imul ebx,475
	add ebx,edi
	sub ebx,60672
	
	jae YUV_to_RGB_asm2
	mov ebx, 0
	jmp YUV_to_RGB_asm21

YUV_to_RGB_asm2:
	cmp ebx,0xFFFF
	jbe YUV_to_RGB_asm21
	mov ebx,0xFF00
	
YUV_to_RGB_asm21:
	and ebx,0xFF00
	shr ebx,8
	
	imul edx,120
	sub edi,edx
	imul esi,48
	sub edi,esi
	add edi,21632
	
	bt edi,31
	jae YUV_to_RGB_asm3
	mov edi, 0
	jmp YUV_to_RGB_asm31
	
YUV_to_RGB_asm3:
	cmp edi,0xFFFF
	jbe YUV_to_RGB_asm31
	mov edi, 0xFF00
	
YUV_to_RGB_asm31:
	and edi,0xFF00
	
	or eax,edi
	or eax,ebx
	
	ret

;extern int freerdp_image_yuv_to_xrgb_asm(unsigned char *pDstData,unsigned char **pSrcData,int nWidth,int nHeight);
	global freerdp_image_yuv_to_xrgb_asm
freerdp_image_yuv_to_xrgb_asm:
	push rbp
	mov rbp, rsp
			;cWidth: cx
	sub rsp,72	;pDstData,pSrcData[3],nWidth,nHeight,cHeight,scanline,iStride[1]
	push rbx
	
	
	mov [rbp-8],rdi
	
	mov rax,[rsi]
	mov [rbp-16],rax
	mov rax,[rsi+8]
	mov [rbp-24],rax
	mov rax,[rsi+16]
	mov [rbp-32],rax
	
	mov [rbp-40],rdx
	
	
	shr rcx,1	;/2
	mov [rbp-48],rcx
	
	
	shl rdx,2
	mov [rbp-64],rdx
	
	
	mov rax,[rbp-48]
	mov [rbp-56],rax
	
	
	mov [rbp-72],r8
	mov rax,[rbp-40]
	shl dword [rbp-72],1
	sub [rbp-72],rax

	shr rax,1
	sub r9,rax
	
freerdp_image_yuv_to_xrgb_asm_loopH:
	mov rcx,[rbp-40]
	shr rcx,1
	
	
freerdp_image_yuv_to_xrgb_asm_loopW:
	mov rax,[rbp-16]
	mov edi,[rax]
	and edi,0xFF
	
	mov rax,[rbp-24]
	mov esi,[rax]
	and esi,0xFF
	
	mov rax,[rbp-32]
	mov edx,[rax]
	and edx,0xFF
	
	call YUV_to_RGB_asm
	
	mov rbx,[rbp-8]
	mov [rbx],eax
	
	
	mov rax,[rbp-16]
	mov edi,[rax+r8]
	inc rax
	mov [rbp-16],rax
	and edi,0xFF
	
	mov rax,[rbp-24]
	mov esi,[rax]
	and esi,0xFF
	
	mov rax,[rbp-32]
	mov edx,[rax]
	and edx,0xFF
	
	call YUV_to_RGB_asm
	
	mov rbx,[rbp-8]
	mov rdx,[rbp-64]
	mov [rbx+rdx],eax
	add rbx,4
	mov [rbp-8],rbx
	
	
	mov rax,[rbp-16]
	mov edi,[rax]
	and edi,0xFF
	
	mov rax,[rbp-24]
	mov esi,[rax]
	and esi,0xFF
	
	mov rax,[rbp-32]
	mov edx,[rax]
	and edx,0xFF
	
	call YUV_to_RGB_asm
	
	mov rbx,[rbp-8]
	mov [rbx],eax
	
	
	mov rax,[rbp-16]
	mov edi,[rax+r8]
	inc rax
	mov [rbp-16],rax
	and edi,0xFF
	
	mov rax,[rbp-24]
	mov esi,[rax]
	inc rax
	mov [rbp-24],rax
	and esi,0xFF
	
	mov rax,[rbp-32]
	mov edx,[rax]
	inc rax
	mov [rbp-32],rax
	and edx,0xFF
	
	call YUV_to_RGB_asm

	mov rbx,[rbp-8]
	mov rdx,[rbp-64]
	mov [rbx+rdx],eax
	add rbx,4
	mov [rbp-8],rbx

	dec cx
	jne freerdp_image_yuv_to_xrgb_asm_loopW
	
	
	mov rax,[rbp-8]
	add rax,[rbp-64]
	mov [rbp-8],rax
	
	mov rax,[rbp-16]
	add rax,[rbp-72]
	mov [rbp-16],rax
	
	mov rax,[rbp-24]
	add rax,r9
	mov [rbp-24],rax
	
	mov rax,[rbp-32]
	add rax,r9
	mov [rbp-32],rax
	
	dec qword [rbp-56]
	jne freerdp_image_yuv_to_xrgb_asm_loopH
	
;END
	mov rax,0
END:
	pop rbx
	mov rsp,rbp
	pop rbp
	ret