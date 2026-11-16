#include<stdio.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

void filosofo (char *arg);

int main(void)
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif
	char filo01[20] = "Sócrates";
	char filo02[20] = "Pitágoras";

	filosofo(filo01);
	filosofo(filo02);

	return 0;
}

void filosofo (char *arg) 
{
	for (int i=1; i<=10; i++) {
		printf ("%s estoy comiendo : %d \n", arg, i);
	}
}
