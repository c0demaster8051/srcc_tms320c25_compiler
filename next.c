
// токенизатор общается с внешним миром только через глобальные переменные
// использует:
// source_ptr - указатель на исходный текст для чтения следующего токена, который двигает только вперёд
// curr_token - возвращает в ней только что обнаруженный токен, представляющий собой или
// символ исходного текста с кодом меньше 128, (верхние символы и русские буквы в исходнике допустимы только в комментах)
// или высокоуровневое разобранное значение с кодом от 128 и выше, их описание от младшего к старшему:
//		number_token - найдено число константа, разобранное значение в token_curr_value
//		identifier_token - найдено имя, возвращает указатель token_idtab_ptr на найденное или свежезаписанное в таблицу имя
//		assign_token - "="
//		condition_token - "?"
//		log_or_token - "||"
//		log_and_token - "&&"
//		bin_or_token - "|" 
//		bin_xor_token - "^"
//		bin_and_token - "&"
//		equal_token - "=="
//		noequal_token - "!="
//		less_token - "<"
//		greater_token - ">"
//		less_or_eq_token - "<="
//		greater_or_eq_token - ">="
//		shift_left_token - "<<"
//		shift_right_token - ">>"
//		add_token - "+"
//		sub_token - "-"
//		mul_token - "*"
//		div_token - "/"
//		mod_token - "%"
//		inc_token - "++"
//		dec_token - "--"
//		bracket_token - "["


void next()
{
char *tmp_ptr; // временный указатель для вычисления длины токена
int tmp_data_ptr; // временно храним индекс массива сегмента данных

//char debug_dump[128];
//char debug_count;

// бесконечный цикл парсинга файла исходника, выходим из которого только
// наткнувшись на определённый токен (и опчионально обработав его)
// curr_token в процессе исполнения меняет содержимое (и смысл того что в нём лежит)
// в начале цикла в нём лежит символ по указателю source_ptr
curr_token_ptr=source_ptr; // сохраняем указатель на первый символ разбираемого токена
while (curr_token = *source_ptr)
	{
	++source_ptr;
	// Проверка на перевод строки
	if (curr_token == '\n') ++line;
	else if (curr_token == '#') // дефайны и прочие директивы пропускаем до окончания строки или всего исходника
		{
		while (*source_ptr!=0 && *source_ptr!='\n') ++source_ptr;
		}
	// проверка на буквы и нижнее подчёркивание
	// си не допускает начало слова (имени функции или переменной) с цифр
	else if ((curr_token >= 'a' && curr_token <= 'z') || (curr_token >= 'A' && curr_token <= 'Z') || curr_token == '_')
		{
		tmp_ptr=source_ptr-1; // временная переменная, нужна для вычисления длины слова
		// магическим костылём вычисляем хеш слова, нужен для уникального идентификатора токена
		while ((*source_ptr>='a' && *source_ptr<='z') || (*source_ptr>='A' && *source_ptr<='Z') || (*source_ptr>='0' && *source_ptr<='9') || *source_ptr=='_') curr_token=curr_token*147+*source_ptr++;
		// влево на 6 бит, в освободившихся длина слова
		curr_token=(curr_token<<6)+(source_ptr-tmp_ptr);
		// token_idtab - указатель на начало массива идентификаторов
		// token_idtab_ptr - указатель поиска по массиву
		token_idtab_ptr = token_idtab; // текущий указатель на начало массива
		// если там уже лежит токен - эта позиция будет не 0,
		// иначе - дошли до конца массива, и наш токен уникальный
		// позиция token_id_define хранит уникальный номер, тип или класс токена, отличный от нуля
		while (token_idtab_ptr[token_id_define])
			{
			// если хеш совпадает с уже имеющимся в массиве...
			// возможна гипотетическая ситуация, когда у разных слов одинаковый хеш (невероятно, но факт)
			// так что на всякий случай сравниваем и само слово через memcmp()
			// позиция token_id_name идентификатора хранит указатель на впервые встреченное уникальное слово			
			if (curr_token == token_idtab_ptr[token_id_hash] && !memcmp((char *)token_idtab_ptr[token_id_name], tmp_ptr, source_ptr - tmp_ptr))
				{
				// совпали хеш и имя, суём в curr_token уникальный номер токена и выходим
				curr_token = token_idtab_ptr[token_id_define];
				return;
				};
			// сдвигаем указатель на размер идентификатора токена, для сравнения со следующим элементом 
			token_idtab_ptr = token_idtab_ptr + token_id_size;
			}
		// если дошли сюда - наш токен уникальный
		token_idtab_ptr[token_id_name] = (int)tmp_ptr; // добавляем указатель на его впервые встреченное имя
		token_idtab_ptr[token_id_namelenght]=(int)(source_ptr-tmp_ptr);
		token_idtab_ptr[token_id_hash] = curr_token; // его хеш
		curr_token = token_idtab_ptr[token_id_define] = identifier_token; // номер (тип, класс) ещё не присвоен
		curr_token_end=source_ptr; // запоминаем длину токена
		return; // выходим
		}
	// наткнулись на число?
	else if (curr_token >= '0' && curr_token <= '9')
		{
		// вычисляем его значение
		// не забываем что source_ptr смотрит на следующий от лежащего в curr_token символ
		// если это десятичное число - первая цифра не "0"
		if (token_curr_value = curr_token - '0')
			{
			// разбираем десятичную строку на запчасти пока не закончатся цифры
			while (*source_ptr >= '0' && *source_ptr <= '9') token_curr_value = token_curr_value * 10 + *source_ptr++ - '0';
			}
		// если первая цифра "0" и далее символ "x" - разбираем так-же HEX число
		else if (*source_ptr == 'x' || *source_ptr == 'X')
			{
			while ((curr_token = *++source_ptr) && ((curr_token >= '0' && curr_token <= '9') || (curr_token >= 'a' && curr_token <= 'f') || (curr_token >= 'A' && curr_token <= 'F')))
				{
				token_curr_value = token_curr_value * 16 + (curr_token & 15) + (curr_token >= 'A' ? 9 : 0);
				};
			}
		// если начинается с "0", но не HEX - значит восьмеричное
		else
			{
			// разбор по аналогии с десятичной
			while (*source_ptr >= '0' && *source_ptr <= '7') token_curr_value = token_curr_value * 8 + *source_ptr++ - '0';
			};
		// теперь разобранное число в token_curr_value, сообщаем тому кто нам вызвал что тут число
		curr_token = number_token;
		curr_token_end=source_ptr; // запоминаем длину токена
		return; // и выходим
		}
	// правый слеш
    else if (curr_token == '/')
		{
		// если их 2 подряд - это коммент
		if (*source_ptr == '/')
			{
			// проматываем до конца строки или до нуля
			++source_ptr;
			while (*source_ptr != 0 && *source_ptr != '\n') ++source_ptr;
			}
		else // один слеш - просто деление
			{
			curr_token = div_token;
			return;
			}
		}
	// одинарные или двойные кавычки
    else if (curr_token == '\'' || curr_token == '"')
		{
		// data_ptr указывает на следующий за последним элемент массива данных (строки, массивы)
		// с какого места мы можем сразу писать
		tmp_data_ptr = data_ptr;
		while (*source_ptr != 0 && *source_ptr != curr_token)
			{
			// символы-исключения вроде перевода строки "\n" (пока только он, позже допишу)
			if ((token_curr_value = *source_ptr++) == '\\')
				{
				if ((token_curr_value = *source_ptr++) == 'n') token_curr_value = '\n';
				};
			// если открылись двойные кавычки - это строка
			// строки всегда хранятся в сегменте данных, копируем в массив data_ptr
			if (curr_token == '"')
				{
				#if wordsize>1
				// чёрная магия с запихиванием нескольких байт в одно слово
				// в данном случае LITTLE ENDIAN
				// переменные target_word и shift инициализируются перед компиляцией
				// и после каждого разбора строки в expr()
				target_word|=(token_curr_value<<(8*shift));
				++shift;
				if(shift>=wordsize)
					{
					code_gen[data_ptr++] = target_word;
					target_word=0;
					shift=0;
					};
				#else
				code_gen[data_ptr++]=token_curr_value;
				#endif
				};
			};
		++source_ptr;
		curr_token_end=source_ptr; // запоминаем длину токена
		// если строка - в token_curr_value возвращаем указатель на начало этой строки
		// в противном случае непосредственно значение (для одинарных кавычек)
		if (curr_token == '"') token_curr_value = tmp_data_ptr;
		else curr_token = number_token;
		return; // и выходим
		}
	// тут всё очевидно и просто
	else if (curr_token == '=') { if (*source_ptr == '=') { ++source_ptr; curr_token = equal_token; } else curr_token = assign_token; return; }
    else if (curr_token == '+') { if (*source_ptr == '+') { ++source_ptr; curr_token = inc_token; } else curr_token = add_token; return; }
    else if (curr_token == '-') { if (*source_ptr == '-') { ++source_ptr; curr_token = dec_token; } else curr_token = sub_token; return; }
    else if (curr_token == '!') { if (*source_ptr == '=') { ++source_ptr; curr_token = noequal_token; } return; }
    else if (curr_token == '<') { if (*source_ptr == '=') { ++source_ptr; curr_token = less_or_eq_token; } else if (*source_ptr == '<') { ++source_ptr; curr_token = shift_left_token; } else curr_token = less_token; return; }
    else if (curr_token == '>') { if (*source_ptr == '=') { ++source_ptr; curr_token = greater_or_eq_token; } else if (*source_ptr == '>') { ++source_ptr; curr_token = shift_right_token; } else curr_token = greater_token; return; }
    else if (curr_token == '|') { if (*source_ptr == '|') { ++source_ptr; curr_token = log_or_token; } else curr_token = bin_or_token; return; }
    else if (curr_token == '&') { if (*source_ptr == '&') { ++source_ptr; curr_token = log_and_token; } else curr_token = bin_and_token; return; }
    else if (curr_token == '^') { curr_token = bin_xor_token; return; }
    else if (curr_token == '%') { curr_token = mod_token; return; }
    else if (curr_token == '*') { curr_token = mul_token; return; }
    else if (curr_token == '[') { curr_token = bracket_token; return; }
    else if (curr_token == '?') { curr_token = condition_token; return; }
	// а эти токены будем разбирать на более высоком уровне, next() просто возвращает его как есть
    else if (curr_token == '~' || curr_token == ';' || curr_token == '{' || curr_token == '}' || curr_token == '(' || curr_token == ')' || curr_token == ']' || curr_token == ',' || curr_token == ':' || curr_token == '@') return;
	};
}