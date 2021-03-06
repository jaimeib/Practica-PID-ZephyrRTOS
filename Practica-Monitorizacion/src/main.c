// Zephyr API
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>

//Standard C + POSIX API
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <posix/pthread.h>

//Modules
#include "main.h"
#include "leds.h"
#include "light_sensor.h"
#include "internal_temp.h"
#include "supervisor.h"
#include "mqtt_publisher.h"

//Threads stacks declaration
K_THREAD_STACK_ARRAY_DEFINE(stacks, NUM_THREADS, STACKSIZE);

//Global variables

//Mutex to access the last result stucture
pthread_mutex_t mutex_result;

//Condition variable to signal the supervisor thread
pthread_cond_t cond_result;
bool new_result_for_supervisor;
bool new_result_for_publisher;

//Condition variable to signal main thread when the supervisor thread is ready
pthread_mutex_t mutex_supervisor_ready;
pthread_cond_t cond_supervisor_ready;
bool supervisor_ready;

//Condition variable to signal main thread when the mqtt publisher thread is ready
pthread_mutex_t mutex_publisher_ready;
pthread_cond_t cond_publisher_ready;
bool publisher_ready;

/**
 * 
 */
int main(void)
{
	printf("Starting ... \n");

	//Threads variables:
	pthread_t green_led_thread, light_sensor_thread, internal_temp_thread, supervisor_thread,
		publisher_thread;
	struct sched_param sch_param;
	pthread_attr_t attr;

	//Result variables:
	thread_result_t *last_result = (thread_result_t *)malloc(sizeof(thread_result_t));

	//Mutexes attribute	declaration
	pthread_mutexattr_t mutexattr;

	// Create the mutex attributes object
	pthread_mutexattr_init(&mutexattr);

	// Create the mutex for the result
	if (pthread_mutex_init(&mutex_result, &mutexattr) != 0) {
		printf("Result mutex created\n");
		exit(EXIT_FAILURE);
	}

	// Create the mutex for the sincronization when supervisor thread is ready
	if (pthread_mutex_init(&mutex_supervisor_ready, &mutexattr) != 0) {
		printf("Supervisor mutex created\n");
		exit(EXIT_FAILURE);
	}

	// Create the mutex for the sincronization when publisher thread is ready
	if (pthread_mutex_init(&mutex_publisher_ready, &mutexattr) != 0) {
		printf("Publisher mutex created\n");
		exit(EXIT_FAILURE);
	}

	// Condition variable attributes initialization
	pthread_condattr_t condattr;

	// Create the condition variable attributes object
	pthread_condattr_init(&condattr);

	// Create the condition variable to signal the supervisor & publisher thread
	if (pthread_cond_init(&cond_result, &condattr) != 0) {
		printf("Condition variable to signal the supervisor thread created\n");
		exit(EXIT_FAILURE);
	}

	//Create the condition variable for the sincronization when supervisor thread is ready
	if (pthread_cond_init(&cond_supervisor_ready, &condattr) != 0) {
		printf("Supervisor condition variable created\n");
		exit(EXIT_FAILURE);
	}

	// Create the condition variable for the sincronization when publisher thread is ready
	if (pthread_cond_init(&cond_publisher_ready, &condattr) != 0) {
		printf("Publisher condition variable created\n");
		exit(EXIT_FAILURE);
	}

	//Threads creation:

	// Set the priority of the main program to max_prio-1
	sch_param.sched_priority = (sched_get_priority_max(SCHED_FIFO) - 1);
	if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_param) != 0) {
		printf("Error while setting main thread's priority\n");
		exit(EXIT_FAILURE);
	}

	// Create the thread attributes object
	if (pthread_attr_init(&attr) != 0) {
		printf("Error while initializing attributes\n");
		exit(EXIT_FAILURE);
	}

	// Thread is created dettached
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
		printf("Error in detachstate attribute\n");
		exit(EXIT_FAILURE);
	}

	// The scheduling policy is fixed-priorities, with
	// FIFO ordering for threads of the same priority
	if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0) {
		printf("Error in schedpolicy attribute\n");
		exit(EXIT_FAILURE);
	}

	//SUPERVISOR THREAD:

	// Set the priority of supervisor thread to min_prio+10
	sch_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + 10);
	if (pthread_attr_setschedparam(&attr, &sch_param) != 0) {
		printf("Error in atribute schedparam of Supervisor thread\n");
		exit(EXIT_FAILURE);
	}

	//Set the stack for supervisor thread
	pthread_attr_setstack(&attr, &stacks[0][0], STACKSIZE);

	// Creating thread that supervised the measures
	if (pthread_create(&supervisor_thread, &attr, (void *)supervisor, last_result) != 0) {
		printf("Error: failed to create supervisor thread\n");
		exit(EXIT_FAILURE);
	}

	//PUBLISHER THREAD:

	// Set the priority of MQTT publisher thread to min_prio+9
	sch_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + 9);
	if (pthread_attr_setschedparam(&attr, &sch_param) != 0) {
		printf("Error in atribute schedparam of MQTT publisher thread\n");
		exit(EXIT_FAILURE);
	}

	//Set the stack for MQTT publisher thread
	pthread_attr_setstack(&attr, &stacks[1][0], STACKSIZE);

	// Creating thread that publish the measures using MQTT
	if (pthread_create(&publisher_thread, &attr, (void *)mqtt_publisher, last_result) != 0) {
		printf("Error: failed to create the MQTT publisher thread\n");
		exit(EXIT_FAILURE);
	}

	//WAIT FOR MQTT SUPERVISOR THREAD TO BE READY BEFORE CREATING THE OTHER THREADS:
	pthread_mutex_lock(&mutex_supervisor_ready);
	while (true) {
		if (!supervisor_ready) {
			pthread_cond_wait(&cond_supervisor_ready, &mutex_supervisor_ready);
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&mutex_supervisor_ready);

	//WAIT FOR MQTT PUBLISHER THREAD TO BE READY BEFORE CREATING THE OTHER THREADS:
	pthread_mutex_lock(&mutex_publisher_ready);
	while (true) {
		if (!publisher_ready) {
			pthread_cond_wait(&cond_publisher_ready, &mutex_publisher_ready);
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&mutex_publisher_ready);

	//LED THREAD:

	// Set the priority of LED thread to min_prio+8
	sch_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + 8);
	if (pthread_attr_setschedparam(&attr, &sch_param) != 0) {
		printf("Error in atribute schedparam of LED thread\n");
		exit(EXIT_FAILURE);
	}

	//Set the stack for LED thread
	pthread_attr_setstack(&attr, &stacks[2][0], STACKSIZE);

	// Creating thread that blinks LED
	if (pthread_create(&green_led_thread, &attr, (void *)blink_led, (void *)RED1) != 0) {
		printf("Error: failed to create LED thread\n");
		exit(EXIT_FAILURE);
	}

	//LIGHT SENSOR THREAD:

	// Set the priority of light sensor thread to min_prio+7
	sch_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + 7);
	if (pthread_attr_setschedparam(&attr, &sch_param) != 0) {
		printf("Error in atribute schedparam of Light Sensor thread\n");
		exit(EXIT_FAILURE);
	}

	//Set the stack for light sensor thread
	pthread_attr_setstack(&attr, &stacks[3][0], STACKSIZE);

	// Creating thread that measures light intensity
	if (pthread_create(&light_sensor_thread, &attr, (void *)light_sensor, last_result) != 0) {
		printf("Error: failed to create light sensor thread\n");
		exit(EXIT_FAILURE);
	}

	//INTERNAL TEMPERATURE THREAD:

	// Set the priority of internal temperature sensor thread min_prio+6
	sch_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + 6);
	if (pthread_attr_setschedparam(&attr, &sch_param) != 0) {
		printf("Error in atribute schedparam of Internal Temperature thread\n");
		exit(EXIT_FAILURE);
	}

	//Set the stack for internal temperature sensor thread
	pthread_attr_setstack(&attr, &stacks[4][0], STACKSIZE);

	// Creating thread that measures light intensity
	if (pthread_create(&internal_temp_thread, &attr, (void *)internal_temp, last_result) != 0) {
		printf("Error: failed to create internal temperature thread\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}
