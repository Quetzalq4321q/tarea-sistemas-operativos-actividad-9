#include<stdio.h>
#include<pthread.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// gcc filosofos04.cpp -pthread

#define NUM_FILOSOFOS 5

void *filosofo (void *arg);

int main(void)
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif
	//char filo01[20] = "Sócrates";
	//char filo02[20] = "Pitágoras";

	pthread_t filosofos[NUM_FILOSOFOS];
	int identificadores[NUM_FILOSOFOS];

	int i;

	for (i=0; i<NUM_FILOSOFOS; i++)	{
		identificadores[i] = i;
		pthread_create (&filosofos[i], NULL, &filosofo, &identificadores[i]);
	}


	for (i=0; i<NUM_FILOSOFOS; i++)	{
		pthread_join(filosofos[i], NULL);
	}

	return 0;
}

void *filosofo (void *arg) 
{
	int arg2 = *((int *)arg);
	for (int i=1; true; i++) {
		printf ("Filósofo-%d estoy comiendo : %d \n", arg2, i);
	}
}
