// Компилятор SRCC - simple, retargettable C compiler
//
// в качестве основы использовался код компилятора C4,
// автор Robert Swierczek, ссылка на оригинал:
// https://github.com/rswier/c4
//
// Все последующие копии и модификации этого экземпляра
// распространяются под лицензией GPL v.2



#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>


// следующие дефайны определяют сколько выделять памяти и как вычислять адрес
#define wordsize 2 // размер слова данных целевой архитектуры в байтах
// следующие определения тоже в байтах
#define ptrsize 2 // размер указателя
#define intsize 2 // размер int
#define longsize 4 // размер long
#define floatsize 4 // размер float
// под архитектуры больше 8 бит, но умеющие в байтовую адресацию...
// ДОПИЛИТЕ САМИ! исходник хорошо откомментирован и понятен. 



char *source_ptr; // current position in source code
#if wordsize>1
// для машин разрядностью более 8 бит в одно слово запихивается больше одного байта строки,
// следующие переменные и куски кода в next() и expr() добавляются только для таких архитектур
int target_word; // тут формируем пачку в несколько байт (например 2 для 16-бит машины)
int shift;
#endif
int string_define_flag; // признак того, что в сегмент данных была добавлена строка

int *code_gen, // начало адресного пространства промежуточного кода
	code_ptr, // указатель для компилятора промежуточного кода
	data_ptr; // указатель на сегмент данных промежуточного кода

int *token_idtab; // стартовый адрес таблицы имён токенов
int *token_idtab_ptr; // указатель в таблице на только что распознанный или внесённый в неё токен

int	curr_token; // символ токена (<128) или его распознанное значение из перечисления (>=128)
char *curr_token_ptr; // указатель на его первый символ в исходнике, пригодится для формирования ассемблерного листинга
char *curr_token_end; // указатель на символ за ним
int token_curr_value; // current token value

int	curr_expr_type,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    debug,    // print executed instructions flag
	target, // compile for target arch flag
	code_seg_offset=16,
	stack_seg_offset=2047,
	data_seg_offset=1792;

int	infunc_label_count=0; // счётчик меток для формирования команд кодогенератору



// перечисление типов данных, используется в лексическом анализаторе для хранения текущего типа данных выражения,
// так-же заносится в следующее поле таблицы токенов при объявлении переменных или функций
// token_idtab_ptr[token_type]
enum { CHAR, INT, LONG, FLOAT, PTR };



// перечисление полей таблицы токенов
// для каждого уникального токена создаётся идентификатор, 
// перечисленные поля заполняются как функцией next()
// при обнаружении неизвестного токена (самые очевидные поля)
// так и функцией expr (более высокоуровневый разбор)
// под каждый токен выделено место фиксиированной длины token_id_size
enum
	{
	token_id_define, token_id_hash, token_id_name, token_id_namelenght,
	token_class, token_type, token_value,
	token_class_hidden, token_type_hidden, token_value_hidden,
	token_id_size
	};
// следующие значения заносятся функцией next() при обнаружении неизвестного ей ранее токена
// token_id_define - смотри описание ниже
// token_id_hash - тут очевидно
// token_id_name - указатель на впервые встреченное имя в исходнике, голое число, для вывода не забываем преобразовать тип в указатель (char*)
// token_id_namelenght - длина этого имени (нужно для функций printf("%.*s"))



// перечисление токенов и классов
// используется сразу несколькими функциями, описание далее в файлах исходников
// так-же используется для определения уровня вложенности парсинга выражений
// по алгоритму precedence climbing, значения перечислены в порядке увеличения
// приоритетности обработки от меньшего приоритета к большему
enum
	{
	number_token = 128, func_token_class, syscall_token_class, glo_var_token_class, glo_array_token_class, loc_var_token_class, loc_array_token_class,
	identifier_token, char_token_id, int_token_id, long_token_id, float_token_id, else_token_id, enum_token_id, if_token_id, return_token_id, sizeof_token_id, portwr_token_id, portrd_token_id, while_token_id,
	assign_token, condition_token, log_or_token, log_and_token, bin_or_token, bin_xor_token, bin_and_token,
	equal_token, noequal_token, less_token, greater_token, less_or_eq_token, greater_or_eq_token, shift_left_token,
	shift_right_token, add_token, sub_token, mul_token, div_token, mod_token, inc_token, dec_token, bracket_token
	};
// значения сгруппированы в логическом порядке, например:
// token_idtab_ptr[token_class] - сюда попадают значения первой строки
// token_idtab_ptr[token_id_define] - сюда попадают значения второй строки,
// значение identifier_token второй строки указывает что наткнулись на уникальное
// имя переменной или функции, некоторые из следующих за ним значений второй строки
// заносятся в таблицу токенов на этапе инициализации в main() и stmt()



// перечисление действий для промежуточного результата работы компилятора (инструкции VM)
enum
	{
	opcode_jump, opcode_call, opcode_bz, opcode_bnz, opcode_entry, opcode_directive, opcode_ldlocal, opcode_ldconst,
	opcode_adj, opcode_portwr, opcode_portrd, opcode_leave, opcode_castcharptr, opcode_getchar, opcode_getint, opcode_getlong, opcode_getfloat,
	opcode_pushchar, opcode_pushint, opcode_pushlong, opcode_pushfloat, opcode_intmode, opcode_longmode,
	opcode_push, opcode_not, opcode_or, opcode_xor, opcode_and, opcode_equal_zero, opcode_equal, opcode_noequal,
	opcode_less, opcode_greater, opcode_less_or_eq, opcode_greater_or_eq,
	opcode_shl, opcode_shr, opcode_add, opcode_sub, opcode_mul, opcode_div, opcode_mod,
	opcode_open, opcode_read, opcode_close, opcode_printf, opcode_malloc, opcode_free, opcode_mset, opcode_memcmp, opcode_exit
	};


// токенизатор
#include "next.c"

// лексический анализатор
#include "expr.c"

// анализатор верхнего уровня (if-else-while)
#include "stmt.c"

// генератор кода под целевую архитектуру
//#include "target_no_opt_orig.c"
#include "target.c"


int main(int argc, char **argv)
{
int fd, temp_type, poolsz, *idmain;
int pc, bp, sp, a, cycle; // vm registers
int i, t, dir_arg; // temps


src=0;
debug=0;
target=0;
--argc; ++argv; // скипаем имя файла компилятора
// следующий параметр тире и режим работы или выдача help,
// перед именем файла допускается только один параметр
if (argc>0 && (*argv)[0]=='-')
	{
	if((*argv)[1] == 's') { src = 1; --argc; ++argv; } // выдать листинг инструкций VM
	else if((*argv)[1] == 'd') { debug = 1; --argc; ++argv; } // пошаговое исполнение VM
	else if((*argv)[1] == 't') { target = 1; --argc; ++argv; } // компиляция под целевую архитектуру
	else if((*argv)[1] == 'h') // большой и толстый HELP
		{
		printf(
		"\nFor compile and run on VM without translation to target arch:\n"
		"srcc source_code.c\n\n"
		"list of optional parameters before source_code file name:\n"
		"-s - compile and show list of VM instructions to stdout\n"
		"-d - for debug, compile source_code and run on VM\n"
		"\twith info on every VM execution step\n"
		"-t - compile and translate to target arch instruction set\n\n"
		"example:\nsrcc -t source_code.c [target_codegen_params]\n\n"
		"list of codegen parameters for target code generation:\n"
		"-cs - start of code segment (reset init code)\n"
		"-ds - start of data segment (where compiler saves global vars and const strings)\n"
		"-ss - init stack pointer address (note that stack grows down)\n"
		"\tafter each of the parameters there must be a number!\n");
		return -1;
		}
	else
		{
		printf("unknown parameter: \"%s\", try \"srcc -h\" for help\n", *argv);
		return -1;
		};
	};
// нет параметров? просто выводим подсказку
if (argc==0)
	{
	printf(
	"\nusage: srcc [-s] [-d] [-t] source_file [target_codegen_params]...\n"
	"try \"srcc -h\" for more details\n");
	return -1;
	};
// если только имя файла - просто исполняем на виртуалке
if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }
// можно ляпнуть в терминал
printf("\nSmall, retargettable C compiler by c0de_master\n\n");
// и разбирать параметры кодогенерации
--argc;
++argv;
if(target)
	{
	if(argc==0) // решили компильнуть под реальный проц, но не указали параметры? ошибка
		{
		printf("not defined parameters for target code generation! try srcc -h for more details\n");
		return -1;
		}
	else while(argc && (*argv)[0]=='-')
		{
		if((*argv)[1]=='c' && (*argv)[2]=='s')
			{
			--argc;
			++argv;
			if(argc) code_seg_offset=strtol(*argv, NULL, 0);
			else
				{
				printf("bad code_seg parameter! try srcc -h for more details\n");
				return -1;				
				};
			printf("code_seg: %d (0x%X)\n", code_seg_offset, code_seg_offset);
			--argc;
			++argv;
			}
		else if((*argv)[1]=='d' && (*argv)[2]=='s')
			{
			--argc;
			++argv;
			if(argc) data_seg_offset=strtol(*argv, NULL, 0);
			else
				{
				printf("bad data_seg parameter! try srcc -h for more details\n");
				return -1;				
				};
			printf("data_seg: %d (0x%X)\n", data_seg_offset, data_seg_offset);
			--argc;
			++argv;
			}
		else if((*argv)[1]=='s' && (*argv)[2]=='s')
			{
			--argc;
			++argv;
			if(argc) stack_seg_offset=strtol(*argv, NULL, 0);
			else
				{
				printf("bad stack_seg parameter! try srcc -h for more details\n");
				return -1;				
				};
			printf("stack_seg: %d (0x%X)\n", stack_seg_offset, stack_seg_offset);
			--argc;
			++argv;
			}
		else break;
		};
	if(argc)
		{
		printf("unknown parameter \"%s\" for target code generation! try srcc -h for more details\n", *argv);
		return -1;
		};	
	printf("\n");
	};


// выделяем память
poolsz = 256*1024;
if (!(token_idtab = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; };
if (!(code_gen = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; };
// обнуляем
memset(token_idtab,  0, poolsz);
memset(code_gen,    0, poolsz);


// инициализация таблицы токенизатора захардкоженными значениями
// их порядок в строке соответствует к их перечислению enum в начале файла
source_ptr = "char int long float else enum if return sizeof portwr portrd while open read close printf malloc free memset memcmp exit void main";
i = char_token_id; // заносим в таблицу базовые слова
while (i <= while_token_id)
	{
	next();
	token_idtab_ptr[token_id_define] = i++;
	};
i = opcode_open; // заносим системные функции
while (i <= opcode_exit)
	{
	next();
	token_idtab_ptr[token_class] = syscall_token_class;
	token_idtab_ptr[token_type] = INT;
	token_idtab_ptr[token_value] = i++;
	};
// к вызовам надо обязательно добавить встроенные математические функции,
// типа sin, cos, sqrt и прочее для обработки сигналов
// дополняем запись в таблице для точки входа в main()
// token_idtab_ptr указывает на последний обработанный токен из строки выше
next(); token_idtab_ptr[token_id_define] = char_token_id; // handle void type
next(); idmain = token_idtab_ptr; // keep track of main
idmain[token_value]=-1;

// выделяем память под исходник и читаем
if (!(source_ptr = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
if ((i = read(fd, source_ptr, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
printf("%dK bytes allocated\n", (poolsz*3)/1024);
printf("%d bytes of source code loaded\n", i);
// прочитав файл в массив - завершаем его нулём
source_ptr[i] = 0;
close(fd);


// дошли сюда без ошибок - значит смогли выделить память под всю фигню и открыть исходник
line = 1;
if(target) data_ptr=data_seg_offset;
else data_ptr=16384;
//else data_ptr=(~((-1)<<((wordsize*8)-2)))+1; // на вторую четверть адресного пространства целевой архитектуры
//else data_ptr=poolsz/(sizeof(int)*2);

// из-за того что для компиляции промежуточного кода используется пред-инкремент,
--code_ptr; // сделаем так чтобы первая инструкция была на адресе 0
#if wordsize>1
 // не забываем подготовить next() к приёму строк, если архитектура больше 8 бит
target_word=0;
shift=0;
#endif
// компиляция начинается отсюда
next(); // берём первый токен, и крутимся в цикле
while (curr_token) // пока токен не 0 (не конец файла)
	{
	// временное хранение типа переменной или возвращаемого значения функции
	temp_type = INT; // по дефолту INT
	if (curr_token == char_token_id)
		{
		next();
		temp_type = CHAR;
		}
	else if (curr_token == int_token_id)
		{
		next();
		temp_type = INT;
		}
	else if (curr_token == long_token_id)
		{
		next();
		temp_type = LONG;
		}
	else if (curr_token == float_token_id)
		{
		next();
		temp_type = FLOAT;
		}
	// перечисление
	else if (curr_token == enum_token_id)
		{
		next();
		// листаем токены пока не попадём на фигурную скобку
		// предполагается что у перечисления должно быть имя
		if (curr_token != '{') next();
		if (curr_token == '{')
			{
			next(); // хватаем первый элемент перечисления
			i = 0; // счётчик
			// пока перечисление не закончится закрывающейся фигурной скобкой
			while (curr_token != '}')
				{
				// найденный токен не похож на уникальное имя? вылет с ошибкой
				if (curr_token != identifier_token)
					{
					printf("%d: bad enum identifier %d\n", line, curr_token);
					return -1;
					};
				next(); // хватаем новый токен уже внутри цикла
				// за именем может последовать знак равно
				if (curr_token == assign_token)
					{
					next();
					// если после знака равно не цифра - вылет с ошибкой
					if (curr_token != number_token)
						{
						printf("%d: bad enum initializer\n", line);
						return -1;
						}
					// в счётчик пишем эту цифру
					i = token_curr_value;
					next(); // тут ловим запятую между именами перечисления
					}
				// указатель token_idtab_ptr при парсинге цифр и отдельных знаков не меняется
				token_idtab_ptr[token_class] = number_token;
				token_idtab_ptr[token_type] = INT;
				token_idtab_ptr[token_value] = i++;
				if (curr_token == ',') next();
				}
			// с закрывающейся фигурной скобки прыгаем на новый токен
			next();
			}
		}
	// до завершения выражения точкой с запятой парсим в этом цикле
	while (curr_token != ';' && curr_token != '}')
		{
		curr_expr_type = temp_type;
		// звёздочка - переменная указатель, число звёздочек - глубина вложенности
		// перечисление типов переменных в начале файла устроено так,
		// что у сформированного числа младший бит обозначает тип переменной,
		// а более старшие биты (если не равны 0) показывают что это указатель,
		// в том числе и глубину вложенности указателей
		while (curr_token == mul_token)
			{
			next(); // в цикле все их достаём
			curr_expr_type = curr_expr_type + PTR;
			}
		// если следующий за ней элемент не имя - вылет с ошибкой
		if (curr_token != identifier_token) { printf("%d: bad global declaration\n", line); printf("curr_token=%X\n", curr_token); return -1; }
		// позиция token_class каждого нового имени заполняется далее по коду, и если не пусто
		// значит мы его уже пробегали и оно объявлено дважды, тоже вылет с ошибкой
		if (token_idtab_ptr[token_class]) { printf("%d: duplicate global definition\n", line); return -1; }
		token_idtab_ptr[token_type] = curr_expr_type;
		next();
		// скобка после имени - объявление функции
		if (curr_token == '(')
			{ // function
			// класс токена - функция, в позиции token_value указатель на неё
			token_idtab_ptr[token_class] = func_token_class;
			token_idtab_ptr[token_value] = code_ptr + 1;
			// запомним имя функции, добавим номер токена в аргумент кодогенератору в инструкции entry
			dir_arg=(int)(token_idtab_ptr-token_idtab)/token_id_size; // в дальнейшем оно будет использовано для меток внутри листинга
			next();
			i = 0; // счётчик локальных переменных
			// цикл перечисления входных переменных в заголовке функции,
			// пока не поймаем закрывающуюся скобку
			while (curr_token != ')')
				{
				//curr_expr_type = INT;
				// токен - тип данных следующего объявления
				if (curr_token==char_token_id) temp_type=CHAR;
				else if (curr_token==int_token_id) temp_type=INT;
				else if (curr_token==long_token_id) temp_type=LONG;
				else if (curr_token==float_token_id) temp_type=FLOAT;
				else
					{
					printf("%d: bad data type declaration\n", line);
					return -1;
					};
				next();
				//if (curr_token == int_token_id) next();
				//else if (curr_token == char_token_id) { next(); curr_expr_type = CHAR; }
				while (curr_token == mul_token) { next(); curr_expr_type = curr_expr_type + PTR; }
				if (curr_token != identifier_token) { printf("%d: bad parameter declaration\n", line); return -1; }
				if (token_idtab_ptr[token_class] == loc_var_token_class) { printf("%d: duplicate parameter definition\n", line); return -1; }
				token_idtab_ptr[token_class_hidden] = token_idtab_ptr[token_class]; token_idtab_ptr[token_class] = loc_var_token_class;
				token_idtab_ptr[token_type_hidden]  = token_idtab_ptr[token_type];  token_idtab_ptr[token_type] = curr_expr_type;
				token_idtab_ptr[token_value_hidden]   = token_idtab_ptr[token_value];   token_idtab_ptr[token_value] = i++; // ***********************************************************************
				//
				next();
				if (curr_token == ',') next();
				};
			next();
			// сразу следом фигурная скобка - начало тела функции, иначе вылет с ошибкой
			if (curr_token != '{') { printf("%d: bad function definition\n", line); return -1; }
			// сохраняем количество входных переменных функции +1			
			loc = ++i;
			next();
			// дальше в начале тела функции перечисление локальных переменных
			// в цикле достаём их, пока не перестанут попадаться объявления типов (char, int)
			// если входных переменных было 2, они получат номера соответственно 0 и 1
			// но внутренние локальные переменные пойдут с промежутком +2, и соответственно
			// получат номера 4, 5, 6 и т.д, это нужно чтобы правильно взять их из стека
			while (curr_token == int_token_id || curr_token == char_token_id || curr_token==long_token_id || curr_token==float_token_id)
				{
				// токен - тип данных следующего объявления
				if (curr_token==char_token_id) temp_type=CHAR;
				if (curr_token==int_token_id) temp_type=INT;
				if (curr_token==long_token_id) temp_type=LONG;
				if (curr_token==float_token_id) temp_type=FLOAT;
				//temp_type = (curr_token == int_token_id) ? INT : CHAR;
				next(); // дальше имя или звёздочка в случае указателя
				while (curr_token != ';')
					{
					curr_expr_type = temp_type;
					// считаем звёздочки, если они есть
					while (curr_token == mul_token)
						{
						next();
						curr_expr_type = curr_expr_type + PTR; // и вычисляем глубину вложенности указателей
						};
					// ошибка если это не имя
					if (curr_token != identifier_token) { printf("%d: bad local declaration\n", line); return -1; }
					// ошибка в случае повторного объявления внутри функции
					if (token_idtab_ptr[token_class]==loc_var_token_class || token_idtab_ptr[token_class]==loc_array_token_class) { printf("%d: duplicate local definition\n", line); return -1; }
					// следующий токен или запятая, или квадратная скобка для массива
					next();
					if(curr_token==bracket_token) // тут разбор объявления массива
						{
						if(curr_expr_type >= PTR) // пока что заглушка от указателей, требует проверки
							{
							printf("%d: local arrays of pointers currently not supported!\n", line);
							return -1;
							};
						token_idtab_ptr[token_class] = loc_array_token_class;
						token_idtab_ptr[token_type] = curr_expr_type + PTR;
						next();
						if(curr_token!=number_token || token_curr_value==0)
							{
							printf("%d: bad local array lenght declaration\n", line);
							return -1;
							};
						if(curr_expr_type==CHAR) // для массива типа char выполняется чёрная магия
							{
							// например для 16-бит архитектур в одном слове умещается 2 байта
							// поэтому экономим память
							i=(token_curr_value/wordsize)+i;
							// если длина массива не кратна ширине слова
							if(token_curr_value%wordsize) ++i; // округляем в большую сторону
							}
						else if(curr_expr_type==INT) i = (token_curr_value*intsize)/wordsize+i; // массив int
						else if(curr_expr_type==LONG) i = (token_curr_value*longsize)/wordsize+i; // массив long
						else if(curr_expr_type==FLOAT) i = (token_curr_value*floatsize)/wordsize+i; // массив float
						else i = (token_curr_value*ptrsize)/wordsize+i; // массив указателей (не важно какой вложенности)
						token_idtab_ptr[token_value] = i;
						next();
						if (curr_token != ']')
							{
							printf("%d: close bracket expected\n", line);
							return -1;
							};
						next();
						}
					else // тут добавление одиночной локальной переменной
						{
						i=i+intsize/wordsize;
						 // под одиночный char в любом случае выделяем объём равный слову целевой архитектуры
						if(curr_expr_type==CHAR) ++i;
						else if(curr_expr_type==INT) i = intsize/wordsize+i; // int
						else if(curr_expr_type==LONG) i = longsize/wordsize+i; // long
						else if(curr_expr_type==FLOAT) i = floatsize/wordsize+i; // float
						else if(curr_expr_type>=PTR) i = ptrsize/wordsize+i; // указатель на любой тип
						else
							{
							printf("%d: internal compiler error in data type\n", line);
							return -1;
							};
						token_idtab_ptr[token_class_hidden] = token_idtab_ptr[token_class]; token_idtab_ptr[token_class] = loc_var_token_class;
						token_idtab_ptr[token_type_hidden]  = token_idtab_ptr[token_type];  token_idtab_ptr[token_type] = curr_expr_type;
						token_idtab_ptr[token_value_hidden]   = token_idtab_ptr[token_value];   token_idtab_ptr[token_value] = i;
						};
					if (curr_token == ',') next();
					};
				next();
				};
			// тут формируется код для входа в процедуру
			// sp сейчас смотрит на адрес возврата
			// *--sp = (int)bp; // сохраняем в стек bp
			// bp = sp; // bp используется для временного хранения указателя стека
			// sp = sp - *pc++; // опускаем стек вниз на количество локальных переменных без учёта входных
			code_gen[++code_ptr] = opcode_entry;
			code_gen[++code_ptr] = dir_arg;
			code_gen[++code_ptr] = i - loc;
			// дальше самое интересное - парсинг тела функции
			// счётчик меток внутри функции считает с нуля, т.к. имя
			// каждой метки содержит имя функции и потому уникально
			infunc_label_count=0;
			while (curr_token != '}') stmt();
			// выход из процедуры
			code_gen[++code_ptr] = opcode_leave;
			token_idtab_ptr = token_idtab;
			// внутри функции могут быть объявлены локальные переменные с таким-же названием как у глобальных
			// в случае такого объявления информация о глобальной переменной с таким-же названием "прячется"
			// и потом парсится тело функции
			while (token_idtab_ptr[token_id_define])
				{
				// этот цикл прочёсывает всю таблицу имён и глобальные переменные снова "открываются"
				if (token_idtab_ptr[token_class] == loc_var_token_class)
					{
					token_idtab_ptr[token_class] = token_idtab_ptr[token_class_hidden];
					token_idtab_ptr[token_type] = token_idtab_ptr[token_type_hidden];
					token_idtab_ptr[token_value] = token_idtab_ptr[token_value_hidden];
					}
				// не забываем оставить указатель на пустой позиции за последним элементом
				token_idtab_ptr = token_idtab_ptr + token_id_size;
				};
			}
		// объявление массива
		else if(curr_token==bracket_token)
			{
			if(curr_expr_type >= PTR) // пока что заглушка от указателей, требует проверки
				{
				printf("%d: global arrays of pointers currently not supported!\n", line);
				return -1;
				};
			token_idtab_ptr[token_class] = glo_array_token_class;
			// для массивов типа char пишем адрес умноженный на число байт в разрядности машины
			token_idtab_ptr[token_value] = (curr_expr_type==CHAR) ? data_ptr*wordsize : data_ptr;
			token_idtab_ptr[token_type] += PTR;
			next();
			if(curr_token!=number_token || token_curr_value==0)
				{
				printf("%d: bad global array lenght declaration\n", line);
				return -1;
				};
			// выделяем место в сегменте данных
			// для массива типа char выполняется чёрная магия
			if(curr_expr_type==CHAR) data_ptr = (token_curr_value/wordsize)+(token_curr_value%wordsize)+data_ptr;
			else if(curr_expr_type==INT) data_ptr = (token_curr_value*intsize)/wordsize+data_ptr;
			else if(curr_expr_type==LONG) data_ptr = (token_curr_value*longsize)/wordsize+data_ptr;
			else if(curr_expr_type==FLOAT) data_ptr = (token_curr_value*floatsize)/wordsize+data_ptr;
			else data_ptr = (token_curr_value*ptrsize)/wordsize+data_ptr;
			next();
			if (curr_token != ']')
				{
				printf("%d: close bracket expected\n", line);
				return -1;
				};
			next();
			}
		// объявление переменной
		else
			{
			// до входа в цикл парсинга функции объявляются только глобальные переменные
			// для которых в сегменте данных выделяется место, token_value хранит указатель на переменную
			token_idtab_ptr[token_class] = glo_var_token_class; // класс - простая глобальная переменная
			// задание адреса размещения вручную
			if(curr_token=='@')
				{
				next();
				if(curr_expr_type!=INT) // жёсткое задание адреса доступно только для типа int
					{
					printf("%d: global var address declaration allowed only for int type!\n", line);
					return -1;					
					};
				if(curr_token!=number_token) // за собакой должно быть число (адрес)
					{
					printf("%d: bad global var address declaration\n", line);
					return -1;
					};
				token_idtab_ptr[token_value] = token_curr_value;
				next();
				}
			else
				{
				// для char пишем адрес умноженный на число байт в разрядности машины
				token_idtab_ptr[token_value] = (curr_expr_type==CHAR) ? data_ptr*wordsize : data_ptr;
				// выделяем место под разные типы данных
				if(curr_expr_type>=PTR) data_ptr = (ptrsize/wordsize)+data_ptr; // для указателя
				if(curr_expr_type==INT) data_ptr = (intsize/wordsize)+data_ptr; // int
				if(curr_expr_type==LONG) data_ptr = (longsize/wordsize)+data_ptr; // long
				if(curr_expr_type==FLOAT) data_ptr = (floatsize/wordsize)+data_ptr; // float
				};
			};
		if (curr_token == ',') next();
		};
	next();
	};



printf("Compiled to %d VM words, main() entry point addr: %d\n\n", code_ptr, idmain[token_value]);



char *opcode_names[]=
    {
	"jump","call","bz","bnz","entry","directive",
	"ldlocal","ldconst","adj","portwr","portrd","leave","castcharptr",
	"getchar","getint","getlong","getfloat","pushchar","pushint","pushlong","pushfloat",
	"intmode","longmode","push","not","or","xor","and","equal_zero","equal","noequal",
	"less","greater","less_or_eq","greater_or_eq","shl",
	"shr","add","sub","mul","div","mod",
	"open","read","close","printf",
	"malloc","free","mset","memcmp","exit",
	"undefined_opcode"
	};

if(src)
	{
	printf("*** VM instructions dump ***:\n");
	for (cycle=0; cycle<=code_ptr; ++cycle)
		{
		i=code_gen[cycle];
		printf("%03d> %s", cycle, opcode_names[((i>=(sizeof(opcode_names)-1) || i<0) ? (sizeof(opcode_names)-1) : i)]);
		if (i < opcode_directive)
			{
			dir_arg=code_gen[++cycle];
			if(i==opcode_entry)
				{
				printf(" func name label: %.*s()", token_idtab[(dir_arg*token_id_size)+token_id_namelenght], (char *)token_idtab[(dir_arg*token_id_size)+token_id_name]);
				}
			else if(i==opcode_call)
				{
				printf(" to func: %.*s()", token_idtab[(dir_arg*token_id_size)+token_id_namelenght], (char *)token_idtab[(dir_arg*token_id_size)+token_id_name]);
				}
			else if(i==opcode_jump || i==opcode_bz || i==opcode_bnz) printf(" to label: %d", dir_arg);
			};
		if(i==opcode_directive) printf(" ********** LABEL_NUM: %d\n", code_gen[++cycle]);
		else if (i <= opcode_portrd) printf(" arg %d\n", code_gen[++cycle]); else printf("\n");
		};
	printf("*** dump end ***\n\n");
	};
	




if ((pc = idmain[token_value])<0) { printf("main() not defined\n"); return -1; }
if (src) return 0;

if (target) target_translate();
else
	{
	printf("*** VM start ***\n");
	// setup stack
	// СТЕК РАСТЁТ ВНИЗ
	// SP смотрит на последний помещённый в него элемент
	//sp = stack_size;
	sp = poolsz/sizeof(int);
	bp = sp;
	code_gen[--sp] = opcode_exit; // call exit if main returns
	code_gen[--sp] = opcode_push;
	t = sp;
	code_gen[--sp] = argc;
	code_gen[--sp] = (int)argv;
	code_gen[--sp] = t;
	// run...
	// костыли и палки, палки и костыли... printf хочет указатель на строку (char*),
	char printf_shitty_fix_buffer[128]; // а данные в памяти виртуалки int
	cycle = 0;
	while (1)
		{
		i = code_gen[pc++];
		++cycle;
		if (debug)
			{
			printf("step:%03d pc=%03d sp=%03d bp=%03d a=%010d > %s", cycle, pc-1, sp, bp, a, opcode_names[i]);
			if (i <= opcode_portrd) printf(" %d\n", ((i < opcode_directive) ? code_gen[pc+1] : code_gen[pc])); else printf("\n");
			};
		// команды с двумя аргументами: директива кодогенератору и адрес
		if (i == opcode_jump) pc = code_gen[pc+1]; // jump
		else if (i == opcode_call) { code_gen[--sp] = pc + 2; pc = code_gen[pc+1]; } // jump to subroutine
		else if (i == opcode_bz)  pc = a ? pc+2 : code_gen[pc+1]; // branch if zero
		else if (i == opcode_bnz) pc = a ? code_gen[pc+1] : pc+2; // branch if not zero
		else if (i == opcode_entry) { code_gen[--sp] = bp; bp = sp; ++pc; sp = sp - code_gen[pc++]; } // enter subroutine
		// команды с одним аргументом
		else if (i == opcode_directive) ++pc; // директива кодогенератору, для виртуальной машины это NOP
		else if (i == opcode_ldlocal) a = (int)(bp + code_gen[pc++]); // load local int address
		else if (i == opcode_ldconst) a = code_gen[pc++]; // load global address or immediate
		else if (i == opcode_adj) sp = sp + (code_gen[pc++]); // stack adjust
		else if (i == opcode_portwr) ++pc; // запись в порт (в виртуалке пока не готова
		else if (i == opcode_portrd) ++pc; // чтение из порта, аналогично не готово
		// все следующие команды без аргументов
		else if (i == opcode_leave) { sp = bp; bp = code_gen[sp++]; pc = code_gen[sp++]; } // leave subroutine
		else if (i == opcode_castcharptr) a = a * wordsize; // преобразование в адрес char
		// команды перемещения данных
		// из-за желания сделать компилятор универсальным
		// далее будет много битовой магии
		else if (i == opcode_getchar) // load char
			{
			// магия запихивания в одно слово нескольких байт
			a = (code_gen[a/wordsize]>>((a&(wordsize-1))*8))&0xFF;
			}
		else if (i == opcode_getint)
			{
			// переменная i в этой итерации VM уже отработала своё, можно менять значение
			//t=a; a=0; i=intsize;
			//do
			//	{
			//	a|=(code_gen[t+(i/wordsize)-1])<<((intsize-i)*8);
			//	i=i-wordsize;
			//	}
			//while(i>0);
			t=a; a=0; i=0;
			do
				{
				a|=(code_gen[t+(i/wordsize)])<<((intsize-wordsize-i)*8);
				i=i+wordsize;
				}
			while(i<intsize);
			}
		else if (i == opcode_getlong)
			{
			t=a; a=0; i=0;
			do
				{
				a|=(code_gen[t+(i/wordsize)])<<((longsize-wordsize-i)*8);
				i=i+wordsize;
				}
			while(i<longsize);
			}
		else if (i == opcode_getfloat)
			{
			t=a; a=0; i=0;
			do
				{
				a|=(code_gen[t+(i/wordsize)])<<((floatsize-wordsize-i)*8);
				i=i+wordsize;
				}
			while(i<floatsize);
			}
		else if (i == opcode_pushchar) // store char
			{
			// аналогичная магия для запихивания байта в слово target машины
			// обнуляем нужные 8 бит внутри слова в памяти машины
			code_gen[code_gen[sp]/wordsize]&=(~(0xFF<<((code_gen[sp]&(wordsize-1))*8)));
			// накладываем туда наш CHAR сдвинутый до нужной позиции
			code_gen[code_gen[sp]/wordsize]|=((a&0xFF)<<((code_gen[sp]&(wordsize-1))*8));
			++sp; // не забываем двинуть stack pointer откуда брали указатель
			}
		else if (i == opcode_pushint)
			{
			//code_gen[code_gen[sp++]] = a&(~((-1)<<(wordsize*8))); // store int
			// переменная i в этой итерации уже отработала своё, можно менять значение
			i=0;
			do
				{
				code_gen[code_gen[sp]+(i/wordsize)]=a>>((intsize-wordsize-i)*8);
				i=i+wordsize;
				}
			while(i<intsize);
			++sp;
			}
		else if (i == opcode_pushlong)
			{
			i=0;
			do
				{
				code_gen[code_gen[sp]+(i/wordsize)]=a>>((longsize-wordsize-i)*8);
				i=i+wordsize;
				}
			while(i<longsize);
			++sp;
			}
		else if (i == opcode_pushfloat)
			{
			i=0;
			do
				{
				code_gen[code_gen[sp]+(i/wordsize)]=a>>((floatsize-wordsize-i)*8);
				i=i+wordsize;
				}
			while(i<floatsize);
			++sp;
			}
		else if (i==opcode_intmode || i==opcode_longmode){} // виртуалка пропускает эти команды
		else if (i == opcode_push) code_gen[--sp] = a; // push
		// логические и арифметические команды
		else if (i == opcode_not)  a = ~a;
		else if (i == opcode_or)  a = code_gen[sp++] |  a;
		else if (i == opcode_xor) a = code_gen[sp++] ^  a;
		else if (i == opcode_and) a = code_gen[sp++] &  a;
		else if (i == opcode_equal_zero)  a = a == 0;
		else if (i == opcode_equal)  a = code_gen[sp++] == a;
		else if (i == opcode_noequal)  a = code_gen[sp++] != a;
		else if (i == opcode_less)  a = code_gen[sp++] <  a;
		else if (i == opcode_greater)  a = code_gen[sp++] >  a;
		else if (i == opcode_less_or_eq)  a = code_gen[sp++] <= a;
		else if (i == opcode_greater_or_eq)  a = code_gen[sp++] >= a;
		else if (i == opcode_shl) a = code_gen[sp++] << a;
		else if (i == opcode_shr) a = code_gen[sp++] >> a;
		else if (i == opcode_add) a = code_gen[sp++] +  a;
		else if (i == opcode_sub) a = code_gen[sp++] -  a;
		else if (i == opcode_mul) a = code_gen[sp++] *  a;
		else if (i == opcode_div) a = code_gen[sp++] /  a;
		else if (i == opcode_mod) a = code_gen[sp++] %  a;
		
		//else if (i == opcode_open) a = open((char *)sp[1], *sp);
		//else if (i == opcode_read) a = read(sp[2], (char *)sp[1], *sp);
		//else if (i == opcode_close) a = close(*sp);
		else if (i == opcode_printf)
			{
			// волшебный костыль для использования аргумента
			t = sp + code_gen[pc+1]; // следующей инструкции adj
			i=code_gen[t-1];
			if(code_gen[t-1]<0 || code_gen[t-1]>=(poolsz*wordsize)/sizeof(int))
				{
				printf("HALT: WTF? string pointer fetched from stack for printf is %d\n", i);
				return -1;
				};
			a = (code_gen[i/wordsize]>>((i&(wordsize-1))*8))&0xFF;
			for(t=0; a!=0; ++i)
				{
				a = (code_gen[i/wordsize]>>((i&(wordsize-1))*8))&0xFF;
				printf_shitty_fix_buffer[t++]=(char)a;
				if(t>=sizeof(printf_shitty_fix_buffer))
					{
				printf("HALT: WTF? printf string is too long, %d chars!\n", t);
				return -1;
					};
				}
			printf_shitty_fix_buffer[t]=0;
			t = sp + code_gen[pc+1]; // ************************************
			a = printf(printf_shitty_fix_buffer, code_gen[t-2], code_gen[t-3], code_gen[t-4], code_gen[t-5], code_gen[t-6]);
			}
		//else if (i == opcode_malloc) a = (int)malloc(*sp);
		//else if (i == opcode_free) free((void *)*sp);
		//else if (i == opcode_mset) a = (int)memset((char *)sp[2], sp[1], *sp);
		//else if (i == opcode_memcmp) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
		else if (i == opcode_exit) { printf("\nmain() return=%d, VM cycles count = %d\n", code_gen[sp], cycle); return code_gen[sp]; }
		else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; }
		};
	};

}
