#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

int min(int x,int y)
{
	if(x>y)
	{
		return y;
	}
	else
	{
		return x;
	}
}

struct station
{
  pthread_mutex_t lock;
  pthread_cond_t is_train_arrived;
  pthread_cond_t is_train_full;
  pthread_cond_t is_passengers_seated;
  int passengers_outside;
  int passengers_inside;
  int empty_seats;
};

struct train_args 
{
	struct station *station;
	int free_seats;
};

volatile int threads_completed = 0;
volatile int load_train_returned = 0;

void init_lock(struct station *station)
{
	pthread_mutex_init(&station->lock , NULL);
	pthread_cond_init(&station->is_train_arrived ,NULL);
	pthread_cond_init(&station->is_passengers_seated,NULL);


	station->passengers_inside=0;
    station->passengers_outside = 0;
    station->empty_seats = 0;

}

void station_load_train(struct station *station, int count)
{

    pthread_mutex_lock(&station->lock);

    station->empty_seats = count;

    while(station->empty_seats > 0 && station->passengers_outside > 0){

            pthread_cond_broadcast(&station->is_train_arrived);
            pthread_cond_wait(&station->is_passengers_seated , &station->lock);

    }

    station->empty_seats = 0;
	pthread_mutex_unlock(&station->lock);
}


void station_wait_for_train(struct station *station)
{
    pthread_mutex_lock(&station->lock);
	station->passengers_outside++;

	while(station->passengers_inside == station->empty_seats){
        pthread_cond_wait(&station->is_train_arrived , &station->lock);
	}

	station->passengers_inside++;
	station->passengers_outside--;
	pthread_mutex_unlock(&station->lock);
}

void station_on_board(struct station *station)
{
	pthread_mutex_lock(&station->lock);
	station->passengers_inside--;
	station->empty_seats--;



	if((station->empty_seats == 0) || (station->passengers_inside == 0))
	{
        pthread_cond_signal(&station->is_passengers_seated);
	}

	pthread_mutex_unlock(&station->lock);

}

void* passenger_thread(void *arg)
{
	struct station *station = (struct station*)arg;
	station_wait_for_train(station);
	__sync_add_and_fetch(&threads_completed, 1);
	return NULL;
}


void* load_train_thread(void *args)
{
	struct train_args *ltargs = (struct train_args*)args;
	station_load_train(ltargs->station, ltargs->free_seats);
	load_train_returned = 1;
	return NULL;
}
int main()
{
	struct station station;
	init_lock(&station);

	station_load_train(&station, 0);
	station_load_train(&station, 10);
	int i;
	int total_passengers;
	printf("\t\t\t\t\t\tWelcome to IndianRailways");
	printf("\nenter total passengers:");
	scanf("%d",&total_passengers);
	int passengers_left = total_passengers;
	for (i = 0; i < total_passengers; i++) 
	{
		pthread_t tid;
		int ret = pthread_create(&tid, NULL, passenger_thread, &station);
		if (ret != 0) {
			perror("pthread_create");
			exit(1);
		}
	}
	station_load_train(&station, 0);
	int total_passengers_boarded = 0;
	int max_free_seats_per_train;
	printf("enter max free seats:");
	scanf("%d",&max_free_seats_per_train);
	
	int pass = 0;
	int tno=0;
	while (passengers_left > 0) 
	{


		int free_seats = rand() % max_free_seats_per_train;
		tno=tno+1;
		printf("Train %d Arriving at station having %d free seats.\n",tno, free_seats);
		load_train_returned = 0;
		struct train_args args = { &station, free_seats };
		pthread_t lt_tid;
		int ret = pthread_create(&lt_tid, NULL, load_train_thread, &args);
		if (ret != 0) 
		{
			perror("pthread_create");
			exit(1);
		}

		int threads_to_reap = min(passengers_left, free_seats);
		int threads_reaped = 0;
		while (threads_reaped < threads_to_reap) 
		{
			if (load_train_returned) 
			{
				fprintf(stderr, "Error: Train departed early without loading completely!\n");
				exit(1);
			}
			if (threads_completed > 0) 
			{
				if ((pass % 2) == 0)
				{
					usleep(rand() % 2);
				}
				threads_reaped++;
				station_on_board(&station);
				__sync_sub_and_fetch(&threads_completed, 1);
			}
		}

		for (i = 0; i < 1000; i++) 
		{
			if (i > 50 && load_train_returned)
				break;
			usleep(1000);
		}

		if (!load_train_returned) 
		{
			fprintf(stderr, "Error: train failed to return\n");
			exit(1);
		}
		while (threads_completed > 0) 
		{
			threads_reaped++;
			__sync_sub_and_fetch(&threads_completed, 1);
		}

		passengers_left -= threads_reaped;
		total_passengers_boarded += threads_reaped;
		if(threads_to_reap==free_seats)
		{
			printf("Train %d departed station having %d new passenger(s) %s\n",
			tno,threads_to_reap,
			(threads_to_reap != threads_reaped) ? " =====" : "");
		}
		else
		{
			printf("Train %d departed station having %d new passenger(s) %s\n",tno,
			free_seats,
			(threads_to_reap != threads_reaped) ? " =====" : "");
		}

		if (threads_to_reap != threads_reaped) 
		{
			fprintf(stderr, "Error: Passengers Overloaded!\n");
			exit(1);
		}

		pass++;
	}

	if (total_passengers_boarded == total_passengers) 
	{
		printf("\t\t\t\t\tThank u for using IndianRailways!\n");
		return 0;
	} 
	else 
	{
		fprintf(stderr, "Error: Expected %d Passengers, but got %d!\n",total_passengers, total_passengers_boarded);
		return 1;
	}
}

