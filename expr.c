void expr(int lev)
{
int datasize, t, d, *idtab_ptr;
int cond_true;
int cond_false;
// лексический анализаор
// code_ptr указывает на последнюю сгенерированную команду
// токен равен 0 - баг
if (!curr_token)
	{
	printf("%d: unexpected eof in expression\n", line);
	exit(-1);
	}
// токен - число
else if (curr_token == number_token)
	{
	// генерируем инструкцию загрузки константы
	code_gen[++code_ptr] = opcode_ldconst;
	// и саму константу
	code_gen[++code_ptr] = token_curr_value;
	next();
	curr_expr_type = INT;
	}
// токен - строка
else if (curr_token == '"')
	{
	// генерируем инструкцию загрузки константы
	code_gen[++code_ptr] = opcode_ldconst;	
	// указатели на char, для архитектур больше 8 бит вычисляются особым образом
	// потому что в одном слове хранится больше одного байта
	code_gen[++code_ptr] = token_curr_value*wordsize;
	// строка может быть поделена на части, перенесённые по строкам
	// пролистываем исходник, пока эта нарезанная строка не закончится
	next();
	while (curr_token == '"') next();
	#if wordsize>1
	// эти переменные используются в next()
	// сохраняем последние байты, если длина строки не кратна ширине слова,
	if(shift) code_gen[data_ptr++] = target_word; // в таком случае строка автоматом завершится нулём
	else code_gen[data_ptr++] = 0; // ну или мы сами её завершим
	target_word=0; // не забываем подготовить next() к следующей строке
	shift=0;
	#else
	code_gen[data_ptr++] = 0; // завершаем строку нулём
	#endif
	curr_expr_type = PTR; // тип выражения - указатель на char
	string_define_flag=1; // флаг, чтобы транслятор под целевую архитектуру добавил данные в бинарник
	}
// функция sizeof
// в данном компиляторе она максимально кастрирована, и нужна только для фокуса с самокомпиляцией
else if (curr_token == sizeof_token_id)
	{
	next();
	// должна продолжаться открывающейся скобкой
	if (curr_token == '(') next();
	else
		{
		// в противном случае выходим с ошибкой
		printf("%d: open paren expected in sizeof\n", line);
		exit(-1);
		};	
	curr_expr_type = INT;
	if (curr_token == int_token_id) next();
	else if (curr_token == char_token_id)
		{
		next();
		curr_expr_type = CHAR;
		};
    while (curr_token == mul_token)
		{
		next();
		curr_expr_type = curr_expr_type + PTR;
		};
	if (curr_token == ')') next();
	else
		{
		printf("%d: close paren expected in sizeof\n", line);
		exit(-1);
		};
	code_gen[++code_ptr] = opcode_ldconst;
	code_gen[++code_ptr] = (curr_expr_type == CHAR) ? sizeof(char) : sizeof(int);
	curr_expr_type = INT;
	}
// функция записи в порт
else if (curr_token == portwr_token_id)
	{
	next();
	// должна продолжаться открывающейся скобкой
	if (curr_token == '(') next();
	else
		{
		// в противном случае выходим с ошибкой
		printf("%d: open paren expected in portwr\n", line);
		exit(-1);
		};
	// разбираем первый аргумент
	expr(assign_token);
	if (curr_token!=',')
		{
		printf("%d: there must be 2 parameters in portwr\n", line);
		exit(-1);
		};
	next();
	if (curr_token!=number_token)
		{
		printf("%d: second parameter in portwr must be a number\n", line);
		exit(-1);
		};
	code_gen[++code_ptr] = opcode_portwr;
	code_gen[++code_ptr] = token_curr_value;
	next();
	if (curr_token != ')')
		{
		printf("%d: too much parameters in portwr\n", line);
		exit(-1);		
		};
	next();
	}
// функция чтения из порта
else if (curr_token == portrd_token_id)
	{
	next();
	// должна продолжаться открывающейся скобкой
	if (curr_token == '(') next();
	else
		{
		// в противном случае выходим с ошибкой
		printf("%d: open paren expected in portrd\n", line);
		exit(-1);
		};
	if (curr_token!=number_token)
		{
		printf("%d: parameter in portrd must be a number\n", line);
		exit(-1);
		};
	code_gen[++code_ptr] = opcode_portrd;
	code_gen[++code_ptr] = token_curr_value;
	next();
	if (curr_token != ')')
		{
		printf("%d: too much parameters in portrd\n", line);
		exit(-1);		
		};
	next();
	}
// самое интересное - токен идентификатор
// это имя функции или переменной
else if (curr_token == identifier_token)
	{
	// запоминаем указатель на этот токен, пригодится
	idtab_ptr = token_idtab_ptr;
	next(); // смотрим что идёт после названия (следующий токен)
	// открывается скобка - значит это вызов функции
	if (curr_token == '(')
		{
		next();
		t = 0;
		// пока выражение внутри скобок не закончилось...
		while (curr_token != ')')
			{
			// рекурсивно вызываем сами себя для вычисления выражений
			// результат разбора выражения будет лежать в аккумуляторе
			// даже если тут просто передача переменной
			expr(assign_token);
			// найденные параметры кидаем на стек
			code_gen[++code_ptr] = opcode_push;
			++t; // и запоминаем их количество
			if (curr_token == ',')
			next();
			};
		next();
		// тут вызов системных функций, для них в виртуалке сделаны отдельные инструкции,
		// которые кодогенератор заменит на вызовы и переходы на готовые ассемблерные куски кода
		if (idtab_ptr[token_class] == syscall_token_class) code_gen[++code_ptr] = idtab_ptr[token_value];
		// до того как вызвать функцию, она должна быть объявлена
		// и её имени должен быть присвоен класс func_token_class
		else if (idtab_ptr[token_class] == func_token_class)
			{
			// формируем инструкции вызова этой функции
			code_gen[++code_ptr] = opcode_call;
			code_gen[++code_ptr] = (int)(idtab_ptr-token_idtab)/token_id_size;
			code_gen[++code_ptr] = idtab_ptr[token_value];
			}
		else
			{
			// класс отличается - это не функция, вылет с ошибкой
			printf("%d: bad function call\n", line);
			exit(-1);
			};
		if (t)
			{
			// после возврата из функции, она вернёт значение в аккумуляторе
			// поднимаем стек на число переданных функции параметров t,
			// они нам больше не нужны
			code_gen[++code_ptr] = opcode_adj;
			code_gen[++code_ptr] = t;
			};
		curr_expr_type = idtab_ptr[token_type];
		}
	// проверяем, является ли токен константой, например перечисленной через enum
	else if (idtab_ptr[token_class] == number_token)
		{
		code_gen[++code_ptr] = opcode_ldconst;
		code_gen[++code_ptr] = idtab_ptr[token_value];
		curr_expr_type = INT;
		}
	// значит всё-таки просто переменная, зохавываем значение в аккумулятор
    else
		{
		// сначала надо найти адрес переменной
		// если она локальная
		if (idtab_ptr[token_class] == loc_var_token_class)
			{
			// получаем адрес через указатель номера переменной (адресация относительно регистра bp)
			code_gen[++code_ptr] = opcode_ldlocal;
			code_gen[++code_ptr] = loc - idtab_ptr[token_value];
			// и далее команда взятия значения по адресу
			// если локальная переменная char - начинаются НЮАНСЫ, адрес надо умножить
			// на число байт в разрядности машины, для формирования адреса char есть особая инструкция
			if(idtab_ptr[token_type]==CHAR)
				{
				code_gen[++code_ptr] = opcode_castcharptr;
				code_gen[++code_ptr] = opcode_getchar;
				}
			else if(idtab_ptr[token_type]==INT) code_gen[++code_ptr] = opcode_getint;
			else if(idtab_ptr[token_type]==LONG) { printf("%d: LONG not allowed in this version!\n", line); exit(-1); } // затычка
			else if(idtab_ptr[token_type]==FLOAT) { printf("%d: FLOAT not allowed in this version!\n", line); exit(-1); }
			else if(idtab_ptr[token_type]>=PTR) code_gen[++code_ptr] = opcode_getint;
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); }
			}
		// если глобальная
		else if (idtab_ptr[token_class] == glo_var_token_class)
			{
			// адрес считать не надо, его подставит компилятор
			code_gen[++code_ptr] = opcode_ldconst;
			code_gen[++code_ptr] = idtab_ptr[token_value];
			if(idtab_ptr[token_type]==CHAR)	code_gen[++code_ptr] = opcode_getchar;
			else if(idtab_ptr[token_type]==INT) code_gen[++code_ptr] = opcode_getint;
			else if(idtab_ptr[token_type]==LONG) { printf("%d: LONG not allowed in this version!\n", line); exit(-1); } // затычка
			else if(idtab_ptr[token_type]==FLOAT) { printf("%d: FLOAT not allowed in this version!\n", line); exit(-1); }
			else if(idtab_ptr[token_type]>=PTR) code_gen[++code_ptr] = opcode_getint;
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); };			
			}		
		// если локальный массив
		else if (idtab_ptr[token_class] == loc_array_token_class)
			{
			// получаем адрес через указатель номера переменной (адресация относительно регистра bp)
			code_gen[++code_ptr] = opcode_ldlocal;
			code_gen[++code_ptr] = loc - idtab_ptr[token_value];
			// если локальный массив char - снова НЮАНСЫ
			if(idtab_ptr[token_type]==PTR) code_gen[++code_ptr] = opcode_castcharptr;
			// брать значение не надо, это будет сделано при обработке скобок
			}
		// если глобальный массив
		else if (idtab_ptr[token_class] == glo_array_token_class)
			{
			// для глобального массива компилятор ТОЛЬКО подставит адрес
			// уже с учётом типа данных (char это или что-то другое)
			code_gen[++code_ptr] = opcode_ldconst;
			code_gen[++code_ptr] = idtab_ptr[token_value];
			}
		// или непонятная необъявленная шняга, вылет с ошибкой
		else
			{
			printf("%d: undefined variable\n", line);
			exit(-1);
			};
		// для простых переменных вставляется инструкция
		// взятия данных в аккумулятор по адресу из него-же,
		// если это массив - то инструкция не добавляется, т.к. следом пойдёт
		// квадратная скобка и разбор содержимого
		// ********************** ВАЖНЫЙ МОМЕНТ ************************
		// если следующий после имени переменной токен будет assign_token,
		// то инструкция взятия по адресу будет ЗАМЕНЕНА НА PUSH в теле блока while
		// в конце этой функции, чтобы сохранить в стеке адрес переменной
		curr_expr_type = idtab_ptr[token_type];
		};
	}
// токен открывающаяся скобка, далее ожидается
// что-то связанное с вложенными выражениями
else if (curr_token == '(')
	{
	next();
	// если за скобкой следует int или char, то тут походу преобразование типов
	if (curr_token == int_token_id || curr_token == char_token_id)
		{
		// временно сохраняем тип переменной
		t = (curr_token == int_token_id) ? INT : CHAR;
		next();
		// звёздочка? значит тут у нас указатель int* или char*
		while (curr_token == mul_token)
			{
			next();
			// сколько звёздочек - столько и уровней вложенности
			t = t + PTR;
			};
		// преобразование типа закрывается скобкой,
		if (curr_token == ')') next();
		else
			{
			// иначе - вылет с ошибкой
			printf("%d: bad cast\n", line);
			exit(-1);
			};
		// рекурсивно вызываем себя с высоким уровнем inc_token, выше только dec_token и bracket_token
		// expr() будет продолжать обработку только при равном или более высоком уровне
		// иначе - обработает один токен и выйдет
		expr(inc_token);
		curr_expr_type = t;
		}
	else
		{
		// а отсюда реально вложенное выражение, поэтому рекурсивно вызываем expr()
		expr(assign_token);
		if (curr_token == ')') next();
		else
			{
			printf("%d: close paren expected\n", line);
			exit(-1);
			};
		};
	}
// токен - звёздочка, взятие по указателю
else if (curr_token == mul_token) 
	{
	// берём следующий токен
	next();
	// тут происходит рекурсивная магия, следующий токен может снова оказаться звёздочкой
	// и мы окажемся в ЭТОМ-ЖЕ МЕСТЕ но во вложенном вызове
	// за звёздочкой может оказаться токен инкремент или декремент, расположенный перед именем указателя,
	// значит надо выполнить это действие с указателем ДО взятия данных по его адресу
	// соответственно рекурсивно вызываем себя с высоким уровнем inc_token, выше только dec_token и bracket_token
	expr(inc_token);
	// 
	if (curr_expr_type >= PTR) curr_expr_type = curr_expr_type - PTR;
	else
		{
		printf("%d: bad dereference\n", line);
		exit(-1);
		};
	code_gen[++code_ptr] = (curr_expr_type == CHAR) ? opcode_getchar : opcode_getint;
	}
// токен амперсанд
// взятие адреса переменной
else if (curr_token == bin_and_token)
	{
	next();
	expr(inc_token); // магия рекурсии
	// предыдущий вызов expr() формирует команды "взять адрес переменной" и следом "взять значение по адресу"
	// последнюю инструкцию отменяем, нам нужен только адрес
	if (code_gen[code_ptr] == opcode_getchar || code_gen[code_ptr] == opcode_getint) --code_ptr;
	else
		{
		printf("%d: bad address-of\n", line);
		exit(-1);
		};
	curr_expr_type = curr_expr_type + PTR;
	}
// токен восклицательный знак, инверсия истинности логического выражения
else if (curr_token == '!')
	{
	next();
	expr(inc_token);
	code_gen[++code_ptr] = opcode_equal_zero;
	curr_expr_type = INT;
	}
// токен тильда, инверсия числа
else if (curr_token == '~')
	{
	next();
	expr(inc_token);
	code_gen[++code_ptr] = opcode_not;
	curr_expr_type = INT;
	}
// токен плюс
// инструкции для сложения формируются в блоке while в конце функции,
// описание смотри там (блок if с тем-же токеном), а конкретно этот код
// бесполезен, но дополняет токен sub_token, смотри описание в следующем if-else
else if (curr_token == add_token)
	{
	next();
	// вложенный вызов, проверяем есть ли инкремент/декремент
	// или взятие значения из массива
	expr(inc_token);
	curr_expr_type = INT;
	}
// токен минус
// операция вычитания разбирается ниже в блоке while, этот код работает только
// когда надо сменить знак выражения, например a=b*-c,
// для значения "c" в данном примере будет получено обратное значение
else if (curr_token == sub_token)
	{
	next();
	if (curr_token == number_token)
		{
		// для константы просто положим её обратное значение
		code_gen[++code_ptr] = opcode_ldconst;
		code_gen[++code_ptr] = -token_curr_value;
		next();
		}
	else
		{
		// выполним разбор выражения
		expr(inc_token);
		// и поменяем знак результата разбора (обратное значение)
		code_gen[++code_ptr] = opcode_not;
		code_gen[++code_ptr] = opcode_push;
		code_gen[++code_ptr] = opcode_ldconst;
		code_gen[++code_ptr] = 1;
		code_gen[++code_ptr] = opcode_add;
		// старый вариант пока оставлю
		//code_gen[++code_ptr] = opcode_ldconst;
		//code_gen[++code_ptr] = -1; // если минус - одно из слагаемых умножить на -1
		//code_gen[++code_ptr] = opcode_push;
		//expr(inc_token);
		//code_gen[++code_ptr] = opcode_mul;
		};
	curr_expr_type = INT;
	}
// пред инкремент и декремент
else if (curr_token == inc_token || curr_token == dec_token)
	{
	// сохраняем действие, пригодится далее для генерации кода
	t = curr_token;
	// следующий токен, должна быть переменная
	// генерируется код взятия адреса переменной,
	// и потом взятия значения по адресу
	next();
	expr(inc_token);
	// отменим инструкцию взятия значения переменной по адресу,
	// сохраним её адрес в стек и только потом возьмём значение для действия
	if (code_gen[code_ptr] == opcode_getchar)
		{
		code_gen[code_ptr] = opcode_push;
		code_gen[++code_ptr] = opcode_getchar;
		}
	else if (code_gen[code_ptr] == opcode_getint)
		{
		code_gen[code_ptr] = opcode_push;
		code_gen[++code_ptr] = opcode_getint;
		}
	else // ну или вылетим с ошибкой, если там шняга
		{
		printf("%d: bad lvalue in pre-increment\n", line);
		exit(-1);
		};
	code_gen[++code_ptr] = opcode_push; // сохраним значение переменной в стек
	code_gen[++code_ptr] = opcode_ldconst; // возьмём константу "единицу"
	// если указатель - инкремент или декремент на размер его данных
	if(curr_expr_type<=PTR) code_gen[++code_ptr]=1; // просто данные, а так-же частный случай для char, для него адрес считается по другому!
	else if(curr_expr_type==(PTR+INT)) code_gen[++code_ptr]=intsize/wordsize; // указатель на int
	else if(curr_expr_type==(PTR+LONG)) code_gen[++code_ptr]=longsize/wordsize; // указатель на long
	else if(curr_expr_type==(PTR+FLOAT)) code_gen[++code_ptr]=floatsize/wordsize; // указатель на float
	else code_gen[++code_ptr]=ptrsize/wordsize; // ни один из предыдущих вариантов - значит указатель на указатель
	code_gen[++code_ptr] = (t == inc_token) ? opcode_add : opcode_sub; // плюс или минус константы со значением со стека, результат в аккумуляторе
	code_gen[++code_ptr] = (curr_expr_type == CHAR) ? opcode_pushchar : opcode_pushint; // сохраним результат по ранее сохранённому адресу переменной
	}
else
	{
	printf("%d: bad expression\n", line);
	exit(-1);
	};
while (curr_token >= lev)
	{
	// "precedence climbing" or "Top Down Operator Precedence" method
	t = curr_expr_type;
	// токен знак равно, присваивание
	if (curr_token == assign_token)
		{
		next();
		// до токена присваивания должно лежать имя переменной или массива, которое в конце сформирует инструкцию
		// opcode_getchar или opcode_getint для взятия значения переменной по адресу, лежащему в аккумуляторе
		// мы эту инструкцию отменим, а адрес просто поместим в стек
		if (code_gen[code_ptr] == opcode_getchar || code_gen[code_ptr] == opcode_getint) code_gen[code_ptr] = opcode_push;
		else
			{
			// ну или вылетим с ошибкой
			printf("%d: bad value in assignment\n", line);
			exit(-1);
			};
		expr(assign_token);
		curr_expr_type = t;
		// записать значение аккумулятора по адресу с вершины стека
		code_gen[++code_ptr] = (curr_expr_type == CHAR) ? opcode_pushchar : opcode_pushint;
		}
	// токен знак вопрос, тернарный оператор
	else if (curr_token == condition_token)
		{
		next();
		cond_false=infunc_label_count++;
		cond_true=infunc_label_count++;
		// в аккумуляторе значение переменной или результат выражения, проверяем
		code_gen[++code_ptr] = opcode_bz;
		code_gen[++code_ptr] = cond_false; // переход на ложную часть оператора для кодогенератора
		d = ++code_ptr; // и адрес для VM
		// отсюда разбор выражения истинной части оператора
		expr(assign_token);
		if (curr_token == ':') next();
		else
			{
			printf("%d: conditional missing colon\n", line);
			exit(-1);
			};
		code_gen[d] = code_ptr + 4;
		code_gen[++code_ptr] = opcode_jump;
		code_gen[++code_ptr] = cond_true; // выход с истинной части оператора
		d = ++code_ptr; // перепрыгиваем его ложную часть
		// метка ложной части оператора, сюда прыгаем если выражение условия не истина
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = cond_false;
		// и разбор её выражения ложной части
		expr(condition_token);
		code_gen[d] = code_ptr + 1;
		// метка конца тернарного оператора, для перехода сюда с истинной части
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = cond_true;
		}
	// токен || логическое или
	else if (curr_token == log_or_token)
		{
		next();
		// тут будет метка, выделим ей номер
		cond_true=infunc_label_count++;
		// логика строится на том, что если первое выражение истинно,
		// то формируемый код пропускает проверку следующих выражений
		code_gen[++code_ptr] = opcode_bnz;
		code_gen[++code_ptr] = cond_true;
		d = ++code_ptr;
		expr(log_and_token);
		code_gen[d] = code_ptr + 1;
		curr_expr_type = INT;
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = cond_true;
		}
	// токен && логическое и
	else if (curr_token == log_and_token)
		{
		next();
		// тут будет метка, выделим ей номер
		cond_false=infunc_label_count++;
		// аналогично предыдущему, если первое выражение ложно,
		// то формируемый код пропускает проверку следующих выражений
		code_gen[++code_ptr] = opcode_bz;
		code_gen[++code_ptr] = cond_false;
		d = ++code_ptr;
		expr(bin_or_token);
		code_gen[d] = code_ptr + 1;
		curr_expr_type = INT;
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = cond_false;
		}
	// со следующими итак всё очевидно, смотри систему команд VM в функции main()
	else if (curr_token == bin_or_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(bin_xor_token);
		code_gen[++code_ptr] = opcode_or;
		curr_expr_type = INT;
		}
	else if (curr_token == bin_xor_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(bin_and_token);
		code_gen[++code_ptr] = opcode_xor;
		curr_expr_type = INT;
		}
	else if (curr_token == bin_and_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(equal_token);
		code_gen[++code_ptr] = opcode_and;
		curr_expr_type = INT;
		}
	else if (curr_token == equal_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(less_token);
		code_gen[++code_ptr] = opcode_equal;
		curr_expr_type = INT;
		}
	else if (curr_token == noequal_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(less_token);
		code_gen[++code_ptr] = opcode_noequal;
		curr_expr_type = INT;
		}
	else if (curr_token == less_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(shift_left_token);
		code_gen[++code_ptr] = opcode_less;
		curr_expr_type = INT;
		}
	else if (curr_token == greater_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(shift_left_token);
		code_gen[++code_ptr] = opcode_greater;
		curr_expr_type = INT;
		}
	else if (curr_token == less_or_eq_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(shift_left_token);
		code_gen[++code_ptr] = opcode_less_or_eq;
		curr_expr_type = INT;
		}
	else if (curr_token == greater_or_eq_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(shift_left_token);
		code_gen[++code_ptr] = opcode_greater_or_eq;
		curr_expr_type = INT;
		}		
	else if (curr_token == shift_left_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(add_token);
		code_gen[++code_ptr] = opcode_shl;
		curr_expr_type = INT;
		}
	else if (curr_token == shift_right_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(add_token);
		code_gen[++code_ptr] = opcode_shr;
		curr_expr_type = INT;
		}
	// токен плюс, сложение числа
	else if (curr_token == add_token)
		{
		next();
		// значение первого слагаемого было получено выше в функции, его тип - t
		// сохраняем его в стек и выполняем разбор следующего выражения
		// с более высоким приоритетом mul_token, это позволяет целиком разобрать
		// и вычислить выражения с умножением, делением и другие, смотри перечисление токенов.
		code_gen[++code_ptr] = opcode_push; // первое слагаемое сохраняем в стек
		expr(mul_token);
		if(t>=PTR && curr_expr_type>=PTR) // если оба операнда указатели - ошибка
			{
			printf("%d: invalid operands: addition of pointers\n", line);
			exit(-1);
			}
		else if(curr_expr_type>=PTR)
			{
			// прибавление к указателю происходит с учётом размерности данных, на которые он указывает,
			// но есть особенность: в теории порядок слагаемых не важен, но т.к. мы генерируем код на лету,
			// надо предупредить пользователя что выполняется простое сложение
			printf("%d: WARNING: can't generate valid pointer addition.\n", line);
			printf("For this pointer must be first in expression.\n");
			};
		curr_expr_type = t;
		if (curr_expr_type >= PTR)
			{
			// для указателей на char и на указатель выполняется обычное сложение
			if(curr_expr_type==PTR || curr_expr_type>=(PTR+PTR)) code_gen[++code_ptr] = opcode_add;
			// если размер int равен разрядности машины - тоже просто сложение
			else if(curr_expr_type==(PTR+INT) && intsize==wordsize) code_gen[++code_ptr] = opcode_add;
			// аналогично для long
			else if(curr_expr_type==(PTR+LONG) && longsize==wordsize) code_gen[++code_ptr] = opcode_add;
			// аналогично для float
			else if(curr_expr_type==(PTR+FLOAT) && floatsize==wordsize) code_gen[++code_ptr] = opcode_add;
			else
				{
				code_gen[++code_ptr] = opcode_push; // сохраняем второе в стек для умножения
				code_gen[++code_ptr] = opcode_ldconst;
				if(curr_expr_type==(PTR+INT)) code_gen[++code_ptr]=intsize/wordsize; // указатель на int
				else if(curr_expr_type==(PTR+LONG)) code_gen[++code_ptr]=longsize/wordsize; // указатель на long
				else if(curr_expr_type==(PTR+FLOAT)) code_gen[++code_ptr]=floatsize/wordsize; // указатель на float
				else code_gen[++code_ptr]=ptrsize/wordsize; // ни один из предыдущих вариантов - значит указатель на указатель
				code_gen[++code_ptr] = opcode_mul; // теперь второе слагаемое подогнано под размер данных
				code_gen[++code_ptr] = opcode_add;
				};
			}
		else if(curr_expr_type==CHAR || curr_expr_type==INT) code_gen[++code_ptr] = opcode_add;
		else if(curr_expr_type==LONG) { printf("%d: LONG addition not allowed in this version!\n", line); exit(-1); } // затычка
		else if(curr_expr_type==FLOAT) { printf("%d: FLOAT addition not allowed in this version!\n", line); exit(-1); }
		else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
		}
	else if (curr_token == sub_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(mul_token);
		// вычитание указателей разного типа недопустимо
		if(curr_expr_type>=PTR && t>=PTR && t!=curr_expr_type)
			{
			printf("%d: invalid operands: substraction of not equal types of pointers\n", line);
			exit(-1);
			}
		// вычитание из числа или переменной указателя - тоже шняга какая-то
		else if(t<PTR && curr_expr_type>=PTR)
			{
			printf("%d: invalid operands: substraction pointer from another data type\n", line);
			exit(-1);
			}
		// вычитание указателей одного типа, или вычитание из указателя числа
		// t - первый операнд, лежит в стеке, curr_expr_type - только что разобранный
		else if (t>=PTR)
			{
			// для указателей на char и на указатель выполняется обычное вычитание
			if(curr_expr_type==PTR || curr_expr_type>=(PTR+PTR)) code_gen[++code_ptr] = opcode_sub;
			// если размер int равен разрядности машины - тоже просто вычитание
			else if(curr_expr_type==(PTR+INT) && intsize==wordsize) code_gen[++code_ptr] = opcode_sub;
			// аналогично для long
			else if(curr_expr_type==(PTR+LONG) && longsize==wordsize) code_gen[++code_ptr] = opcode_sub;
			// аналогично для float
			else if(curr_expr_type==(PTR+FLOAT) && floatsize==wordsize) code_gen[++code_ptr] = opcode_sub;
			// вычитание двух указателей когда длина данных не совпадает с разрядностью машины
			else if(curr_expr_type==t)
				{
				code_gen[++code_ptr] = opcode_sub; // выполняем вычитание
				code_gen[++code_ptr] = opcode_push; // сохраняем результат в стек для деления
				code_gen[++code_ptr] = opcode_ldconst; // выбираем длину данных указателя
				if(curr_expr_type==(PTR+INT)) code_gen[++code_ptr]=intsize/wordsize; // указатель на int
				else if(curr_expr_type==(PTR+LONG)) code_gen[++code_ptr]=longsize/wordsize; // указатель на long
				else if(curr_expr_type==(PTR+FLOAT)) code_gen[++code_ptr]=floatsize/wordsize; // указатель на float
				else code_gen[++code_ptr]=ptrsize/wordsize; // ни один из предыдущих вариантов - значит указатель на указатель
				code_gen[++code_ptr] = opcode_div; // теперь в аккумуляторе корректная разница между указателями
				curr_expr_type = INT;
				}
			// вычитание из указателя (тип данных t) числа
			else
				{
				code_gen[++code_ptr] = opcode_push; // сохраняем вычитаемое в стек для умножения
				code_gen[++code_ptr] = opcode_ldconst; // выбираем длину данных указателя
				if(t==(PTR+INT)) code_gen[++code_ptr]=intsize/wordsize; // указатель на int
				else if(t==(PTR+LONG)) code_gen[++code_ptr]=longsize/wordsize; // указатель на long
				else if(t==(PTR+FLOAT)) code_gen[++code_ptr]=floatsize/wordsize; // указатель на float
				else code_gen[++code_ptr]=ptrsize/wordsize; // ни один из предыдущих вариантов - значит указатель на указатель
				code_gen[++code_ptr] = opcode_mul; // теперь в аккумуляторе корректное вычитаемое с учётом длины данных
				code_gen[++code_ptr] = opcode_sub; // выполняем вычитание
				curr_expr_type = t; // результат вычитания - указатель, просто сдвинутый
				};
			}
		else
			{
			// простое вычитание числа
			curr_expr_type=t;
			if(curr_expr_type==CHAR || curr_expr_type==INT) code_gen[++code_ptr] = opcode_sub;
			else if(curr_expr_type==LONG) { printf("%d: LONG addition not allowed in this version!\n", line); exit(-1); } // затычка
			else if(curr_expr_type==FLOAT) { printf("%d: FLOAT addition not allowed in this version!\n", line); exit(-1); }
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
			//code_gen[++code_ptr] = opcode_sub;
			};
		}
	else if (curr_token == mul_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(inc_token);
		code_gen[++code_ptr] = opcode_mul;
		curr_expr_type = INT;
		}
	else if (curr_token == div_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(inc_token);
		code_gen[++code_ptr] = opcode_div;
		curr_expr_type = INT;
		}
	else if (curr_token == mod_token)
		{
		next();
		code_gen[++code_ptr] = opcode_push;
		expr(inc_token);
		code_gen[++code_ptr] = opcode_mod;
		curr_expr_type = INT;
		}
	// пост инкремент и декремент
	// выполняется немножко костыльно, но работает ******************************************************************************************* МОЖНО СИЛЬНО ОПТИМИЗИРОВАТЬ, ДОБАВИТЬ В VM ИНСТРУКЦИЮ POP
	else if (curr_token == inc_token || curr_token == dec_token)
		{
		// отменяем предыдущую инструкцию взятия значения по адресу
		// сохраняем адрес в стек и только потом берём значение
		if (code_gen[code_ptr] == opcode_getchar)
			{
			code_gen[code_ptr] = opcode_push;
			code_gen[++code_ptr] = opcode_getchar;
			}
		else if (code_gen[code_ptr] == opcode_getint)
			{
			code_gen[code_ptr] = opcode_push;
			code_gen[++code_ptr] = opcode_getint;
			}
		else
			{
			printf("%d: bad lvalue in post-increment\n", line);
			exit(-1);
			};
		// сохраняем значение
		code_gen[++code_ptr] = opcode_push;
		code_gen[++code_ptr] = opcode_ldconst; // ************************************************************************************************** НЕ ДОДЕЛАНО, ДОПИЛИТЬ VM ПОД ВЗЯТИЕ КОНСТАНТЫ РАЗНЫХ ТИПОВ
		// // берём число, насколько выполнить инкремент или декремент, с учётом размера данных если это указатель
		if(curr_expr_type<=PTR) datasize=1; // просто данные, а так-же частный случай для char, для него адрес считается по другому!
		else if(curr_expr_type==(PTR+INT)) datasize=intsize/wordsize; // указатель на int
		else if(curr_expr_type==(PTR+LONG)) datasize=longsize/wordsize; // указатель на long
		else if(curr_expr_type==(PTR+FLOAT)) datasize=floatsize/wordsize; // указатель на float
		else datasize=ptrsize/wordsize; // ни один из предыдущих вариантов - значит указатель на указатель
		code_gen[++code_ptr] = datasize;
		// дальше будет расписана сложная конструкция под разные типы данных
		// сначала выполняем действие с значением переменной, взятым из стека, на вершине стека остался адрес
		if(curr_token==inc_token)
			{
			if(curr_expr_type==CHAR || curr_expr_type==INT || curr_expr_type>=PTR) code_gen[++code_ptr] = opcode_add;
			else if(curr_expr_type==LONG) { printf("%d: LONG addition not allowed in this version!\n", line); exit(-1); } // затычка
			else if(curr_expr_type==FLOAT) { printf("%d: FLOAT addition not allowed in this version!\n", line); exit(-1); }
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
			}
		else
			{
			if(curr_expr_type==CHAR || curr_expr_type==INT || curr_expr_type>=PTR) code_gen[++code_ptr] = opcode_sub;
			else if(curr_expr_type==LONG) { printf("%d: LONG substraction not allowed in this version!\n", line); exit(-1); } // затычка
			else if(curr_expr_type==FLOAT) { printf("%d: FLOAT substraction not allowed in this version!\n", line); exit(-1); }
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
			};			
		//code_gen[++code_ptr] = (curr_token == inc_token) ? opcode_add : opcode_sub;
		// сохраняем результат работы с переменной по её ранее сохранённому адресу
		if(curr_expr_type==CHAR) code_gen[++code_ptr] = opcode_pushchar;
		else if(curr_expr_type==INT) code_gen[++code_ptr] = opcode_pushint;
		else if(curr_expr_type==LONG) { printf("%d: LONG not allowed in this version!\n", line); exit(-1); } // затычка
		else if(curr_expr_type==FLOAT) { printf("%d: FLOAT not allowed in this version!\n", line); exit(-1); }
		else if(curr_expr_type>=PTR) code_gen[++code_ptr] = opcode_pushint;
		else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
		//code_gen[++code_ptr] = (curr_expr_type == CHAR) ? opcode_pushchar : opcode_pushint;
		code_gen[++code_ptr] = opcode_push; code_gen[++code_ptr] = opcode_ldconst; // ***************************************************************************** АНАЛОГИЧНО С ВЗЯТИЕМ КОНСТАНТЫ
		code_gen[++code_ptr] = datasize;
		// в аккумуляторе всё так-же хранится изменённая переменная, обратным действием возвращаем всё как было
		if(curr_token==inc_token)
			{
			if(curr_expr_type==CHAR || curr_expr_type==INT || curr_expr_type>=PTR) code_gen[++code_ptr] = opcode_sub;
			else if(curr_expr_type==LONG) { printf("%d: LONG substraction not allowed in this version!\n", line); exit(-1); } // затычка
			else if(curr_expr_type==FLOAT) { printf("%d: FLOAT substraction not allowed in this version!\n", line); exit(-1); }
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
			}
		else
			{
			if(curr_expr_type==CHAR || curr_expr_type==INT || curr_expr_type>=PTR) code_gen[++code_ptr] = opcode_add;
			else if(curr_expr_type==LONG) { printf("%d: LONG addition not allowed in this version!\n", line); exit(-1); } // затычка
			else if(curr_expr_type==FLOAT) { printf("%d: FLOAT addition not allowed in this version!\n", line); exit(-1); }
			else { printf("%d: internal compiler error in data type\n", line); exit(-1); };
			}
		// в аккумуляторе теперь значение переменной до инкремента/декремента
		//code_gen[++code_ptr] = (curr_token == inc_token) ? opcode_sub : opcode_add;
		next();
		}
	else if (curr_token == bracket_token)
		{
		next();
		// если переменная - массив, то таблица токенов хранит адрес его первого элемента,
		// и выше расположенный в этой функции код возьмёт значение первого элемента
		// инструкцией getint, заменим её на push чтобы сохранить только адрес
		//if(idtab_ptr[token_class]==glo_array_token_class || idtab_ptr[token_class]==loc_array_token_class)
		//	{
		//	code_gen[code_ptr] = opcode_push;
		//	}
		// в обычном случае сохраняем содержимое указателя или адрес первого элемента массива
		code_gen[++code_ptr] = opcode_push;
		expr(assign_token); // после разбора выражения в аккумуляторе будет индекс массива
		if (curr_token == ']') next();
		else
			{
			printf("%d: close bracket expected\n", line);
			exit(-1);
			};
		if (t < PTR)
			{
			printf("%d: pointer type expected\n", line);
			exit(-1);
			};
		if(t==PTR) // указатель на char - отдельный случай
			{
			// просто прибавляем индекс к адресу первого элемента на вершине стека
			// хитрость в том что адрес на char не всегда соответствует реальному адресу памяти,
			// для машин с 16-битным словом адрес уже умножен компилятором на 2
			// для 8-бит машин адрес остаётся как есть
			// *** ВНИМАНИЕ ***
			// я хотел сделать компилятор универсальным, но не тестил его с другими таргетами, кроме tms320c25,
			// для 8-бит машин код вычисления адреса может быть ошибочным, проверяйте сами!
			code_gen[++code_ptr] = opcode_add;
			code_gen[++code_ptr] = opcode_getchar;
			//curr_expr_type=CHAR;
			}
		else
			{
			// дальше расписаны варианты под разную разрядность машины,
			// компилятор должен быть универсальным
			if(t==(PTR+INT)) // указатель на int
				{
				if(intsize==wordsize)
					{
					// прибавляем индекс к адресу первого элемента на вершине стека
					code_gen[++code_ptr] = opcode_add;
					code_gen[++code_ptr] = opcode_getint;
					}
				else
					{
					code_gen[++code_ptr] = opcode_push; // сохраним индекс в стек
					code_gen[++code_ptr] = opcode_ldconst; // возьмём константу 
					code_gen[++code_ptr] = intsize/wordsize; // размер данных int
					code_gen[++code_ptr] = opcode_mul; // умножим индекс на размер данных, теперь в аккуме правильное смещение от базы
					code_gen[++code_ptr] = opcode_add; // прибавить базовый адрес, куда смотрит указатель или первый элемент массива
					code_gen[++code_ptr] = opcode_getint; // взять значение 
					};
				}
			else if(t==(PTR+LONG)) // указатель на long
				{
				if(longsize==wordsize)
					{
					// прибавляем индекс к адресу первого элемента на вершине стека
					code_gen[++code_ptr] = opcode_add;
					//code_gen[++code_ptr] = opcode_getlong; // ****************************** ЕЩЁ НЕ ГОТОВО
					}
				else
					{
					code_gen[++code_ptr] = opcode_push; // сохраним индекс в стек
					code_gen[++code_ptr] = opcode_ldconst; // возьмём константу 
					code_gen[++code_ptr] = longsize/wordsize; // размер данных int
					code_gen[++code_ptr] = opcode_mul; // умножим индекс на размер данных, теперь в аккуме правильное смещение от базы
					code_gen[++code_ptr] = opcode_add; // прибавить базовый адрес, куда смотрит указатель или первый элемент массива
					//code_gen[++code_ptr] = opcode_getlong; // взять значение *************************** ЕЩЁ НЕ ГОТОВО
					};
				}
			else if(t==(PTR+FLOAT)) // указатель на float
				{
				if(floatsize==wordsize)
					{
					// прибавляем индекс к адресу первого элемента на вершине стека
					code_gen[++code_ptr] = opcode_add;
					//code_gen[++code_ptr] = opcode_getfloat; // ****************************** ЕЩЁ НЕ ГОТОВО
					}
				else
					{
					code_gen[++code_ptr] = opcode_push; // сохраним индекс в стек
					code_gen[++code_ptr] = opcode_ldconst; // возьмём константу 
					code_gen[++code_ptr] = floatsize/wordsize; // размер данных int
					code_gen[++code_ptr] = opcode_mul; // умножим индекс на размер данных, теперь в аккуме правильное смещение от базы
					code_gen[++code_ptr] = opcode_add; // прибавить базовый адрес, куда смотрит указатель или первый элемент массива
					//code_gen[++code_ptr] = opcode_getfloat; // взять значение *************************** ЕЩЁ НЕ ГОТОВО
					};
				}
			else // указатель на указатель, указатель как правило равен размеру int данной архитектуры
				{
				if(ptrsize==wordsize)
					{
					// прибавляем индекс к адресу первого элемента на вершине стека
					code_gen[++code_ptr] = opcode_add;
					code_gen[++code_ptr] = opcode_getint;
					}
				else
					{
					code_gen[++code_ptr] = opcode_push; // сохраним индекс в стек
					code_gen[++code_ptr] = opcode_ldconst; // возьмём константу 
					code_gen[++code_ptr] = ptrsize/wordsize; // размер данных int
					code_gen[++code_ptr] = opcode_mul; // умножим индекс на размер данных, теперь в аккуме правильное смещение от базы
					code_gen[++code_ptr] = opcode_add; // прибавить базовый адрес, куда смотрит указатель или первый элемент массива
					code_gen[++code_ptr] = opcode_getint; // взять значение
					};
				};
			};
		// в обработку массива мы входили с типом указатель {data_type}+PTR (указатель на первый элемент массива)
		// взяв значение надо указать его тип данных (вычесть PTR)
		curr_expr_type = t-PTR;
		}
	else
		{
		printf("%d: compiler error curr_token=%d\n", line, curr_token);
		exit(-1);
		};
	}
}