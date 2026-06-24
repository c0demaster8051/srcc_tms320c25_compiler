; TMS320C25 C compiler by c0de_master
.ORG 0h
	b _rst_init


.ORG 010h
_rst_init:
	larp 1 ; use AR1 as stack pointer
	lrlk 1,0800h ; top of stack addr
	ldpk 0h
	rovm ; reset overflow mode
	rsxm ; reset sign-extention mode
	call _main
	b _rst_init



_main:
	mar *- ; function entry
	popd *-
	sar 0,*
	sar 1,060h
	lrlk 0,0Eh
	mar *0-
	lar 0,060h
	lalk 128 ; load const
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lalk 0 ; load const
	lar 2,*+,2 ; **sp++=a store ACC to addr on top of stack
	sacl *,0,1
	lalk 12 ; load const
	sacl 060h ; write data in ACC to port
	out 060h,15
	lalk 128 ; load const
	mar *- ; *--sp=a push ACC to stack
	sacl *
	in 060h,15 ; read data from port to ACC
	lac 060h
	lar 2,*+,2 ; **sp++=a store ACC to addr on top of stack
	sacl *,0,1
	lrlk 2,-6 ; save local var addr to stack
	larp 2
	mar *0+,1
	mar *-
	sar 2,*
	lac *
	lalk 3584 ; load const
	lar 2,*+,2 ; **sp++=a store ACC to addr on top of stack
	sacl *,0,1
	lrlk 2,-2 ; save local var addr to stack
	larp 2
	mar *0+,1
	mar *-
	sar 2,*
	lac *
	lalk 0 ; load const
	lar 2,*+,2 ; **sp++=a store ACC to addr on top of stack
	sacl *,0,1
	lalk 3596 ; load const
	mar *- ; *--sp=a push ACC to stack
	sacl *
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	adrk 1 ; stack adjust
_main_lbl_0:
	lrlk 2,-4 ; save local var addr to stack
	larp 2
	mar *0+,1
	mar *-
	sar 2,*
	lac *
	lrlk 2,-6 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lrlk 2,-2 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	adds *+ ; a=a+(*sp++) add top of stack to ACC
	call __getchar_macro ; get char to ACC on addr in ACC
	lar 2,*+,2 ; **sp++=a store ACC to addr on top of stack
	sacl *,0,1
	bz _main_lbl_1
	lalk 3616 ; load const
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lrlk 2,-4 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	mar *- ; *--sp=a push ACC to stack
	sacl *
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	adrk 2 ; stack adjust
	lrlk 2,-14 ; load local var addr to ACC
	larp 2
	mar *0+,1
	sar 2,060h
	lac 060h
	sfl
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lrlk 2,-2 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	adds *+ ; a=a+(*sp++) add top of stack to ACC
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lrlk 2,-4 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	call __pushchar_macro ; **sp++=a store low ACC to char-addr on top of stack
	lrlk 2,-2 ; save local var addr to stack and load value to ACC
	larp 2
	mar *0+
	lac *,0,1
	mar *-
	sar 2,*
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lalk 1 ; load const
	adds *+ ; a=a+(*sp++) add top of stack to ACC
	lar 2,*+,2 ; **sp++=a store ACC to addr on top of stack
	sacl *,0,1
	b _main_lbl_0
_main_lbl_1:
	lalk 3620 ; load const
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lrlk 2,-2 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	mar *- ; *--sp=a push ACC to stack
	sacl *
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	adrk 2 ; stack adjust
	lrlk 2,-14 ; load local var addr to ACC
	larp 2
	mar *0+,1
	sar 2,060h
	lac 060h
	sfl
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lrlk 2,-2 ; load local var value to ACC
	larp 2
	mar *0+
	lac *,0,1
	adds *+ ; a=a+(*sp++) add top of stack to ACC
	mar *- ; *--sp=a push ACC to stack
	sacl *
	lalk 0 ; load const
	call __pushchar_macro ; **sp++=a store low ACC to char-addr on top of stack
	lalk 3632 ; load const
	mar *- ; *--sp=a push ACC to stack
	sacl *
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	adrk 1 ; stack adjust
	lrlk 2,-14 ; load local var addr to ACC
	larp 2
	mar *0+,1
	sar 2,060h
	lac 060h
	sfl
	mar *- ; *--sp=a push ACC to stack
	sacl *
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	nop ; blob printf
	adrk 1 ; stack adjust
	b __return_macro


__getchar_macro:
	sfr ; a=(*(a>>1))>>((a&1)*8) get char to ACC on addr in ACC
	sacl 060h
	lar 2,060h
	larp 2
	bnc __getchar_macro_lo
	lt *,1
	mpyk 100h
	sph 060h
	lac 060h
	ret
__getchar_macro_lo:
	lac *,0,1
	andk 0FFh
	ret
__pushchar_macro:
	andk 0FFh
	sacl 060h
	lt 060h
	lac *+,0,2
	sfr
	sacl 060h
	lar 2,060h
	lac *,0
	bnc __pushchar_macro_lo
	andk 0FFh
	mpyk 100h
	b __pushchar_macro_common
__pushchar_macro_lo:
	andk 0FF00h
	mpyk 1h
__pushchar_macro_common:
	spl 060h
	or 060h
	sacl *,0,1
	ret
__return_macro:
	sar 0,060h ; function return code
	lar 1,060h
	lar 0,*+
	pshd *+
	ret

.ORG 0700h
.msfirst
.word 06574h, 07473h, 07320h, 07274h, 06E69h, 00067h, 07274h, 02079h
.word 06F74h, 07320h, 06F68h, 02077h, 05453h, 04952h, 0474Eh, 0000Ah
.word 06325h, 0000Ah, 06F63h, 06E75h, 03A74h, 02520h, 00A64h, 00000h
.word 06E61h, 02064h, 06F6Eh, 02077h, 07266h, 06D6Fh, 06420h, 07365h
.word 02074h, 07261h, 06172h, 03A79h, 0000Ah

.END
