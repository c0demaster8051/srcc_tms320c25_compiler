// Тестовый код, для проверки работы компилятора

int timer@0x80; // при работе с этой переменной обращение будет к адресу 0x80

int main()
{
int count;
int value;
char *source;
char dest[16];

timer=0;
portwr(12,15); // запись в порт с адресом 15
timer=portrd(15); // чтение из того-же порта

source="test string";
count=0;
printf("try to show STRING\n");
while(value=source[count])
	{
	printf("%c\n",value);
	dest[count]=value;
	++count;
	};
printf("count: %d\n",count);
dest[count]=0;
printf("and now from dest array:\n");
printf(dest);
}