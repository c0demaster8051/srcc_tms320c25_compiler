

void target_translate()
{
// перечисление задействованных макросов для добавления в код вызовов к ним
enum
	{
	include_equal_zero, include_equal, include_noequal, include_greater_or_eq, include_less,
	include_greater, include_less_or_eq, include_shl, include_shr, include_getchar, include_pushchar
	};

// переменная с флагами использованных макросов
int include_macro_flags=0; // для выборочного их добавления в конце листинга
int fetch_ptr=0; // счётчик читаемых инструкций
int opcode_arg; // аргумент инструкции VM
int curr_func=0; // номер токена с именем функции в таблице, для создания меток
int target_instr_count=0; // счётчик инструкций целевой архитектуры

// пишем в этот файл
FILE* asm_listing;
// сообщим в консоль что делаем
printf("*** Target: TMS320C25 ***\n");
asm_listing=fopen("example.asm", "wb");
// пошёл товар на экспорт
fprintf(asm_listing,"; TMS320C25 C compiler by c0de_master\n"); // заголовок
fprintf(asm_listing,
	".ORG 0h\n"
	"\tb _rst_init\n\n\n"); // переход на reset_vector
// тут опционально может разместиться таблица векторов прерываний
// отсюда инициализация после сброса и переход на main()
fprintf(asm_listing,
	".ORG 0%Xh\n"
	"_rst_init:\n"
	"\tlarp 1 ; use AR1 as stack pointer\n"
	"\tlrlk 1,0%Xh ; top of stack addr\n"
	"\tldpk 0h\n"
	"\trovm ; reset overflow mode\n"
	"\trsxm ; reset sign-extention mode\n"
	"\tcall _main\n"
	"\tb _rst_init\n", code_seg_offset, stack_seg_offset);

// основной цикл трансляции промежуточного кода в ассемблерный листинг целевой архитектуры
while(fetch_ptr<=code_ptr) switch(code_gen[fetch_ptr++])
{
case opcode_jump:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"\tb _%.*s_lbl_%d\n", token_idtab[(curr_func*token_id_size)+token_id_namelenght], (char *)token_idtab[(curr_func*token_id_size)+token_id_name], opcode_arg);
++target_instr_count;
++fetch_ptr;
break;

case opcode_call:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"\tcall _%.*s ; call to func\n", token_idtab[(opcode_arg*token_id_size)+token_id_namelenght], (char *)token_idtab[(opcode_arg*token_id_size)+token_id_name], opcode_arg);
++target_instr_count;
++fetch_ptr;
break;

case opcode_bz:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"\tbz _%.*s_lbl_%d\n", token_idtab[(curr_func*token_id_size)+token_id_namelenght], (char *)token_idtab[(curr_func*token_id_size)+token_id_name], opcode_arg);
++target_instr_count;
++fetch_ptr;
break;

case opcode_bnz:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"\tbnz _%.*s_lbl_%d\n", token_idtab[(curr_func*token_id_size)+token_id_namelenght], (char *)token_idtab[(curr_func*token_id_size)+token_id_name], opcode_arg);
++target_instr_count;
++fetch_ptr;
break;

case opcode_entry:
opcode_arg=code_gen[fetch_ptr++];
curr_func=opcode_arg;
// вход в функцию, ставим метку с именем аргумента
fprintf(asm_listing,"\n\n\n_%.*s:\n", token_idtab[(opcode_arg*token_id_size)+token_id_namelenght], (char *)token_idtab[(opcode_arg*token_id_size)+token_id_name]);
fprintf(asm_listing,
	"\tmar *- ; function entry\n"
	"\tpopd *-\n"
	"\tsar 0,*\n"
	"\tsar 1,060h\n"
	"\tlrlk 0,0%Xh\n"
	"\tmar *0-\n"
	"\tlar 0,060h\n", ((code_gen[fetch_ptr++])&0xFFFF));
target_instr_count+=7;
break;

case opcode_directive:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"_%.*s_lbl_%d:\n", token_idtab[(curr_func*token_id_size)+token_id_namelenght], (char *)token_idtab[(curr_func*token_id_size)+token_id_name], opcode_arg);
break;

case opcode_ldlocal:
opcode_arg=code_gen[fetch_ptr++];
// попытка в оптимизацию,
// смотрим что идёт следующей командой
if(code_gen[fetch_ptr]==opcode_getint) // дальше идёт взятие по адресу из этой переменной
	{
	// ldlocal
	// getint
	fprintf(asm_listing,
	"\tlrlk 2,%d ; load local var value to ACC\n"
	"\tlarp 2\n"
	"\tmar *0+\n"
	"\tlac *,0,1\n", opcode_arg);
	target_instr_count+=4;
	++fetch_ptr;
	}
else if(code_gen[fetch_ptr]==opcode_push) // дальше идёт сохранение адреса в стек
	{
	// ldlocal
	// push
	// getint
	if(code_gen[fetch_ptr+1]==opcode_getint)
		{
		fprintf(asm_listing,
		"\tlrlk 2,%d ; save local var addr to stack and load value to ACC\n"
		"\tlarp 2\n"
		"\tmar *0+\n"
		"\tlac *,0,1\n"
		"\tmar *-\n"
		"\tsar 2,*\n", opcode_arg);
		target_instr_count+=6;
		fetch_ptr+=2;
		}
	else
		{
		// ldlocal
		// push
		fprintf(asm_listing,
		"\tlrlk 2,%d ; save local var addr to stack\n"
		"\tlarp 2\n"
		"\tmar *0+,1\n"
		"\tmar *-\n"
		"\tsar 2,*\n"
		"\tlac *\n", opcode_arg); // загрузить ещё и в аккум
		target_instr_count+=6;
		++fetch_ptr;
		}
	}
else // стандартный вариант
	{
	fprintf(asm_listing,
	"\tlrlk 2,%d ; load local var addr to ACC\n"
	"\tlarp 2\n"
	"\tmar *0+,1\n"
	"\tsar 2,060h\n"
	"\tlac 060h\n", opcode_arg);
	target_instr_count+=5;
	};
break;

case opcode_ldconst:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"\tlalk %d ; load const\n", opcode_arg);
++target_instr_count;
break;

case opcode_adj:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,"\tadrk %d ; stack adjust\n", opcode_arg);
++target_instr_count;
break;

case opcode_portwr:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,
	"\tsacl 060h ; write data in ACC to port\n"
	"\tout 060h,%d\n", opcode_arg);
break;

case opcode_portrd:
opcode_arg=code_gen[fetch_ptr++];
fprintf(asm_listing,
	"\tin 060h,%d ; read data from port to ACC\n"
	"\tlac 060h\n", opcode_arg);
break;

case opcode_leave:
// пропускаем если подряд идут 2 инструкции возврата, например
// когда делают return со значением в конце тела функции,
// или прыгаем на общий для всех кусок с выходом
if(code_gen[fetch_ptr]!=opcode_leave)
	{
	fprintf(asm_listing,"\tb __return_macro\n");
	++target_instr_count;
	};
break;

case opcode_castcharptr:
fprintf(asm_listing,"\tsfl\n", opcode_arg);
++target_instr_count;
break;

case opcode_getchar:
fprintf(asm_listing, "\tcall __getchar_macro ; get char to ACC on addr in ACC\n");
include_macro_flags|=1<<include_getchar;
++target_instr_count;
break;

case opcode_getint:
fprintf(asm_listing,
	"\tsacl 060h ; a=*a get data to ACC on addr in ACC\n"
	"\tlar 2,060h\n"
	"\tlarp 2\n"
	"\tlac *,0,1\n");
target_instr_count+=4;
break;

case opcode_pushchar:
fprintf(asm_listing, "\tcall __pushchar_macro ; **sp++=a store low ACC to char-addr on top of stack\n");
include_macro_flags|=1<<include_pushchar;
++target_instr_count;
break;

case opcode_pushint:
fprintf(asm_listing,
	"\tlar 2,*+,2 ; **sp++=a store ACC to addr on top of stack\n"
	"\tsacl *,0,1\n");
target_instr_count+=2;
break;

case opcode_push:
fprintf(asm_listing,
	"\tmar *- ; *--sp=a push ACC to stack\n"
	"\tsacl *\n");
target_instr_count+=2;
break;

case opcode_not:
fprintf(asm_listing,"\tcmpl ; a=~a inverse ACC\n");
++target_instr_count;
break;

case opcode_or:
fprintf(asm_listing,"\tor *+ ; a=a|(*sp++) or ACC with top of stack\n");
++target_instr_count;
break;

case opcode_xor:
fprintf(asm_listing,"\txor *+ ; a=a^(*sp++) xor ACC with top of stack\n");
++target_instr_count;
break;

case opcode_and:
fprintf(asm_listing,"\tand *+ ; a=a&(*sp++) and ACC with top of stack\n");
++target_instr_count;
break;

case opcode_equal_zero:
fprintf(asm_listing, "\tcall __equal_zero_macro ; a=a==0 true if ACC equal zero\n");
include_macro_flags|=1<<include_equal_zero;
++target_instr_count;
break;

case opcode_equal:
fprintf(asm_listing, "\tcall __equal_macro ; a=(*sp++)==a true if ACC equal with top of stack\n");
include_macro_flags|=1<<include_equal;
++target_instr_count;
break;

case opcode_noequal:
fprintf(asm_listing, "\tcall __noequal_macro ; a=(*sp++)!=a true if ACC noequal with top of stack\n");
include_macro_flags|=1<<include_noequal;
++target_instr_count;
break;

case opcode_greater_or_eq:
fprintf(asm_listing, "\tcall __greater_or_eq_macro ; a=(*sp++)>=a true if top of stack greater then or equal to ACC\n");
include_macro_flags|=1<<include_greater_or_eq;
++target_instr_count;
break;

case opcode_less:
fprintf(asm_listing, "\tcall __less_macro ; a=(*sp++)<a true if top of stack less then ACC\n");
include_macro_flags|=1<<include_less;
++target_instr_count;
break;

case opcode_greater:
fprintf(asm_listing, "\tcall __greater_macro ; a=(*sp++)>a true if top of stack greater then ACC\n");
include_macro_flags|=1<<include_greater;
++target_instr_count;
break;

case opcode_less_or_eq:
fprintf(asm_listing, "\tcall __less_or_eq_macro ; a=(*sp++)<=a true if top of stack less then or equal to ACC\n");
include_macro_flags|=1<<include_less_or_eq;
++target_instr_count;
break;

case opcode_shl:
fprintf(asm_listing, "\tcall __shl_macro ; a=*(sp++)<<a shift value on top of stack left on ACC bits\n");
include_macro_flags|=1<<include_shl;
++target_instr_count;
break;

case opcode_shr:
fprintf(asm_listing, "\tcall __shr_macro ; a=*(sp++)>>a shift value on top of stack right on ACC bits\n");
include_macro_flags|=1<<include_shr;
++target_instr_count;
break;

case opcode_add:
fprintf(asm_listing,"\tadds *+ ; a=a+(*sp++) add top of stack to ACC\n");
++target_instr_count;
break;

case opcode_sub:
fprintf(asm_listing,
	"\tneg ; a=(*sp++)-a substract ACC from top of stack\n"
	"\tadds *+\n");
target_instr_count+=2;
break;

case opcode_mul:
fprintf(asm_listing,"; blob mul\n");
break;

case opcode_div:
fprintf(asm_listing,"; blob div\n");
break;

case opcode_mod:
fprintf(asm_listing,"; blob mod\n");
break;

case opcode_printf:
fprintf(asm_listing,
	"\tnop ; blob printf\n"
	"\tnop ; blob printf\n"
	"\tnop ; blob printf\n"
	"\tnop ; blob printf\n");
target_instr_count+=4;
break;

case opcode_exit:
fprintf(asm_listing,"; blob exit\n");
break;

default:
printf("Translator error: unknown VM instruction!\n");
//return;
};


fprintf(asm_listing,"\n\n"); // промежуток в пару строк между кодом и макросами

if(include_macro_flags&(1<<include_getchar))
	{
	fprintf(asm_listing,
	"__getchar_macro:\n"
	"\tsfr ; a=(*(a>>1))>>((a&1)*8) get char to ACC on addr in ACC\n"
	"\tsacl 060h\n"
	"\tlar 2,060h\n"
	"\tlarp 2\n"
	"\tbnc __getchar_macro_lo\n"
	"\tlt *,1\n"
	"\tmpyk 100h\n"
	"\tsph 060h\n"
	"\tlac 060h\n"
	"\tret\n"
	"__getchar_macro_lo:\n"
	"\tlac *,0,1\n"
	"\tandk 0FFh\n"
	"\tret\n");
	target_instr_count+=13;
	};

if(include_macro_flags&(1<<include_pushchar))
	{
	fprintf(asm_listing,
	"__pushchar_macro:\n"
	"\tandk 0FFh\n"	// ограничили аккум
	"\tsacl 060h\n"
	"\tlt 060h\n" // перегнали его в T
	"\tlac *+,0,2\n" // взяли адрес со стека, выбор указателя 2
	"\tsfr\n" // сдвинули, владший бит в C
	"\tsacl 060h\n"
	"\tlar 2,060h\n" // перегнали адрес в указатель, дальше read-modify-write
	"\tlac *,0\n" // взяли данные по указателю
	"\tbnc __pushchar_macro_lo\n" // ветвимся по младшему биту
	"\tandk 0FFh\n" // обрезаем слово из массива сверху
	"\tmpyk 100h\n" // записываемый байт сдвигаем вверх на 8 бит
	"\tb __pushchar_macro_common\n"
	"__pushchar_macro_lo:\n"
	"\tandk 0FF00h\n" // обрезаем слово из массива снизу
	"\tmpyk 1h\n" // записываемый байт не сдвигаем, проталкиваем в P
	"__pushchar_macro_common:\n"
	"\tspl 060h\n" // временно сохраняем записываемое
	"\tor 060h\n" // лепим в аккумулятор
	"\tsacl *,0,1\n" // пишем аккум по указателю 2 и переключаемся на стек
	"\tret\n");
	target_instr_count+=18;
	};

if(include_macro_flags&(1<<include_equal_zero))
	{
	fprintf(asm_listing,
	"__equal_zero_macro:\n"
	"\tsacl 060h ; a=a==0 true if ACC equal zero\n"
	"\tlac 060h\n"
	"\tsubk 1\n"
	"\tzac\n"
	"\trol\n"
	"\txork 01h\n"
	"\tret\n");
	target_instr_count+=7;
	};

if(include_macro_flags&(1<<include_equal))
	{
	fprintf(asm_listing,
	"__equal_macro:\n"
	"\tsacl 060h ; a=(*sp++)==a true if ACC equal with top of stack\n"
	"\tlac 060h\n"
	"\txor *+\n"
	"\tsubk 1\n"
	"\tzac\n"
	"\trol\n"
	"\txork 01h\n"
	"\tret\n");
	target_instr_count+=8;
	};

if(include_macro_flags&(1<<include_noequal))
	{
	fprintf(asm_listing,
	"__noequal_macro:\n"
	"\tsacl 060h ; a=(*sp++)!=a true if ACC noequal with top of stack\n"
	"\tlac 060h\n"
	"\txor *+\n"
	"\tsubk 1\n"
	"\tzac\n"
	"\trol\n"
	"\tret\n");
	target_instr_count+=7;
	};

if(include_macro_flags&(1<<include_greater_or_eq))
	{
	fprintf(asm_listing,
	"__greater_or_eq_macro:\n"
	"\tsacl 060h ; a=(*sp++)>=a true if top of stack greater then or equal to ACC\n"
	"\tlac 060h\n"
	"\tneg\n"
	"\tadds *+\n"
	"\tzac\n"
	"\trol\n"
	"\tret\n");
	target_instr_count+=7;
	};

if(include_macro_flags&(1<<include_less))
	{
	fprintf(asm_listing,
	"__less_macro:\n"
	"\tsacl 060h ; a=(*sp++)<a true if top of stack less then ACC\n"
	"\tlac 060h\n"
	"\tneg\n"
	"\tadds *+\n"
	"\tzac\n"
	"\trol\n"
	"\txork 01h\n"
	"\tret\n");
	target_instr_count+=8;
	};

if(include_macro_flags&(1<<include_greater))
	{
	fprintf(asm_listing,
	"__greater_macro:\n"
	"\tsacl 060h ; a=(*sp++)>a true if top of stack greater then ACC\n"
	"\tlac 060h\n"
	"\taddk 01h\n"
	"\tneg\n"
	"\tadds *+\n"
	"\tzac\n"
	"\trol\n"
	"\tret\n");
	target_instr_count+=8;
	};

if(include_macro_flags&(1<<include_less_or_eq))
	{
	fprintf(asm_listing,
	"__less_or_eq_macro:\n"
	"\tsacl 060h ; a=(*sp++)<=a true if top of stack less then or equal to ACC\n"
	"\tlac 060h\n"
	"\taddk 01h\n"
	"\tneg\n"
	"\tadds *+\n"
	"\tzac\n"
	"\trol\n"
	"\txork 01h\n"
	"\tret\n");
	target_instr_count+=9;
	};

if(include_macro_flags&(1<<include_shl))
	{
	fprintf(asm_listing,
	"__shl_macro:\n"
	"\tsacl 060h ; a=*(sp++)<<a shift value on top of stack left on ACC bits\n"
	"\tlac *+,15\n"
	"\trpt 060h\n"
	"\tsfl\n"
	"\tsach 060h\n"
	"\tlac 060h\n"
	"\tret\n");
	target_instr_count+=7;
	};

if(include_macro_flags&(1<<include_shr))
	{
	fprintf(asm_listing,
	"__shr_macro:\n"
	"\tsacl 060h ; a=*(sp++)>>a shift value on top of stack right on ACC bits\n"
	"\tlac *+,1\n"
	"\trpt 060h\n"
	"\tsfr\n"
	"\tret\n");
	target_instr_count+=5;
	};

// добавляем дефолтный для любой программы выход из функции
fprintf(asm_listing,
	"__return_macro:\n"
	"\tsar 0,060h ; function return code\n"
	"\tlar 1,060h\n"
	"\tlar 0,*+\n"
	"\tpshd *+\n"
	"\tret\n\n");
target_instr_count+=5;

// добавляем в исходник строки и константы, если были объявлены
if(string_define_flag)
	{
	fprintf(asm_listing,".ORG 0%Xh\n.msfirst\n", data_seg_offset);
		for(fetch_ptr=data_seg_offset; fetch_ptr<data_ptr; ++fetch_ptr)
		{
		if(((fetch_ptr-data_seg_offset)&0x0007)==0) fprintf(asm_listing,".word 0%04Xh", code_gen[fetch_ptr]);
		else if(((fetch_ptr-data_seg_offset)&0x0007)==7) fprintf(asm_listing,", 0%04Xh\n", code_gen[fetch_ptr]);
		else fprintf(asm_listing,", 0%04Xh", code_gen[fetch_ptr]);
		};
	fprintf(asm_listing,"\n\n");
	};



// директива транслятору ассемблера telemark
fprintf(asm_listing,".END\n");

// это успешный успех
printf("Code translation success!\n");
printf("Program size without init code: %d instructions\n", target_instr_count);
}