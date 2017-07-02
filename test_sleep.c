#include <pthread.h>
#include <stdio.h>
#include <string.h>
static void * thread1(void* arg){
	printf("I am thread 1\n");
	sleep(2);
	printf("I am thread 11\n");
}
static void * thread2(void* arg){
	printf("I am thread 2\n");
	wait(2);
	printf("I am thread 22\n");
}
void main(){
	pthread_t th1,th2;
	int *status;

	int ret=pthread_create(&th1,NULL,thread1,NULL);
	sleep(1);
	printf("I am main1\n");
	pthread_join(th1,(void**)&status);


	ret=pthread_create(&th2,NULL,thread2,NULL);
	wait(2);
	printf("I am main2\n");
	pthread_join(th2,(void**)&status);
}
