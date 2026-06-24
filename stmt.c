void stmt()
{
// адрес начала проверки условий цикла while для возврата после итерации
int loop_start;
// адрес выхода при невыполнении условий if, чтобы перепрыгнуть тело условия
int cond_exit;
// аналогично, но директивы кодогенератору с номерами меток
int dir_loop_start;
int dir_cond_exit;
int dir_else_exit;
// в эту функцию входим для парсинга тела функции (содержимое скобок),
// или аналогичного по смыслу тела условия (тоже в скобках)
if (curr_token == if_token_id)
	{
	next();
	// за "if" всегда следует скобка, иначе вылет с ошибкой
	if (curr_token == '(') next();
	else
		{
		printf("%d: open paren expected\n", line);
		exit(-1);
		};
	// парсим содержимое блока условия
	expr(assign_token);
	// которое аналогично закрывается скобкой
	if (curr_token == ')') next();
	else
		{
		printf("%d: close paren expected\n", line);
		exit(-1);
		};
	dir_cond_exit=infunc_label_count++; // запоминаем номер метки, во вложенных вызовах он может измениться
	code_gen[++code_ptr] = opcode_bz; // инструкция проверки аккумулятора на ==0
	code_gen[++code_ptr] = dir_cond_exit; // выход за тело {} истинного условия
	// в указателе cond_exit временно запомним адреса перехода VM за тело {} для истинного условия
	cond_exit = ++code_ptr; // если условие в скобках () не выполнится
	// рекурсивная магия обработки тела условия
	stmt();
	// при наличии блока else
	if (curr_token == else_token_id)
		{
		// по ранее сохранённому адресу cond_exit положим адрес перехода
		// ЗА следующие команды, там начнётся тело {} для ложного условия
		code_gen[cond_exit] = code_ptr + 4;
		// на эту группу команд мы попадаем после исполнения {} истинного условия,
		// чтобы после исполнения {} истинного перепрыгнуть тело {} ложного
		dir_else_exit=infunc_label_count++;
		code_gen[++code_ptr] = opcode_jump;
		code_gen[++code_ptr] = dir_else_exit; // ***************************************************************************************************ДИРЕКТИВА
		cond_exit = ++code_ptr; // запоминаем место хранения аргумента (адреса) команды opcode_jump
		// не забудем указать кодогенератору где ставить метку выхода за тело истинной части блока if
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = dir_cond_exit; // *****************************************************************************************************ДИРЕКТИВА
		next();
		stmt();
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = dir_else_exit; // *****************************************************************************************************ДИРЕКТИВА		
		}
	else
		{
		code_gen[++code_ptr] = opcode_directive;
		code_gen[++code_ptr] = dir_cond_exit; // *****************************************************************************************************ДИРЕКТИВА
		};
	// пишем адрес следующей инструкции ЗА блоком IF-ELSE
	code_gen[cond_exit] = code_ptr - 1; // указывает на ранее сформированный opcode_directive
	}
else if (curr_token == while_token_id)
	{
	next();
	// временно сохраняем адрес начала кода проверки условия ()
	// чтобы прыгнуть сюда после выполнения тела {}
	loop_start = code_ptr + 1;
	// сообщаем кодогенератору место размещения метки возврата в начало цикла
	dir_loop_start=infunc_label_count++; // запоминаем номер метки
	code_gen[++code_ptr] = opcode_directive;
	code_gen[++code_ptr] = dir_loop_start;
	// и заранее готовим номер метки для выхода из цикла
	dir_cond_exit=infunc_label_count++; // запоминаем номер метки
	if (curr_token == '(') next();
	else
		{
		printf("%d: open paren expected\n", line);
		exit(-1);
		};
	// тут формируется проверка условия, результат которой вернём в аккумуляторе
	expr(assign_token);
	if (curr_token == ')') next();
	else
		{
		printf("%d: close paren expected\n", line);
		exit(-1);
		};
	// инструкция проверки аккумулятора на ==0
	code_gen[++code_ptr] = opcode_bz;
	code_gen[++code_ptr] = dir_cond_exit; // ******************************************************************************************************ДИРЕКТИВА
	// в указателе cond_exit временно запомним место хранения адреса перехода за тело цикла {}
	cond_exit = ++code_ptr; // если условие в скобках () не выполняется
	stmt(); // парсим тело цикла
	// возврат на проверку условия () выполнения итерации
	code_gen[++code_ptr] = opcode_jump;
	code_gen[++code_ptr] = dir_loop_start; // ********************************************************************************************************ДИРЕКТИВА
	code_gen[++code_ptr] = loop_start;
	// тут пишем аргумент (адрес) для команды opcode_bz
	// для перехода за тело цикла в случае ложного условия ()
	code_gen[cond_exit] = code_ptr + 1;
	code_gen[++code_ptr] = opcode_directive;
	code_gen[++code_ptr] = dir_cond_exit;	
	}
else if (curr_token == return_token_id)
	{
	next();
	// если return что-то возвращает - парсим аргумент
	if (curr_token != ';') expr(assign_token);
	// команда выхода из подпрограммы
	code_gen[++code_ptr] = opcode_leave;
	if (curr_token == ';') next();
	// у команды return только один аргумент или выражение, после парсинга которого
	// мы должны остановиться на точке с запятой, иначе - вылет с ошибкой
	else
		{
		printf("%d: semicolon expected\n", line);
		exit(-1);
		};
	}
// парсим содержимое {} функции, цикла, или блока IF-ELSE
else if (curr_token == '{')
	{
	next();
	// рекурсивный вызов, пока {} не закроется
	while (curr_token != '}') stmt();
	next();
	}
else if (curr_token == ';')
	{
	next();
	}
else
	{
	// тут парсим выражения, например "a=a+3;"
	// expr() разбирает ОДНО выражение целиком,
	// останавливаясь на следующем за выражением токеном
	expr(assign_token);
	// в строке допускается только одно выражение заканчивающееся
	// точкой с запятой, иначе - вылет с ошибкой
	if (curr_token == ';') next();
	else
		{
		printf("%d: semicolon expected\n", line);
		exit(-1);
		};
	};
}