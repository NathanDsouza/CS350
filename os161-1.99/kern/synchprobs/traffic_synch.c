#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */




static struct array* carsOrigin;
static struct array* carsDest;
static struct cv *cv;
static struct lock *lock;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
	carsOrigin = array_create();
	carsDest = array_create();
	array_init(carsOrigin);
	array_init(carsDest);
	lock = lock_create("c");
	cv = cv_create("c");
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
 
  lock_destroy(lock);
  cv_destroy(cv);
  array_destroy(carsOrigin);
  array_destroy(carsDest);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */
static bool
rightTurn(Direction origin, Direction destination){
	signed int num = origin -destination;
	return ((num  == 1) || (num == -3));
}

static bool
okayToEnter(Direction origin, Direction destination, struct array* carsOrigin, struct array* carsDest, int i){
	
	unsigned int curOrigin = *(int*)array_get(carsOrigin, i);
	unsigned int curDest = *(int*)array_get(carsDest, i);
	
	return (!((curOrigin == origin && curDest == destination) ||
				(curOrigin == destination && curDest == origin)
				|| (curDest != destination && rightTurn(origin, destination))));
			

}

void
intersection_before_entry(Direction origin, Direction destination) 
{
	
	lock_acquire(lock);
	unsigned int carTotal = array_num(carsOrigin);
	if (carTotal == 0){
		int *org = kmalloc(sizeof(int));
		*org = origin;
		int *dest = kmalloc(sizeof(int));
		*dest = destination;
		
		array_add(carsOrigin, org, NULL);
		array_add(carsDest, dest, NULL);
	}
	else {
		for (unsigned int i = 0; i < carTotal; i++){
			
			if (okayToEnter(origin, destination, carsOrigin, carsDest, i)){
		
			 	cv_wait(cv, lock);
				i = -1;	
				carTotal = array_num(carsOrigin);
			}
		}
		int *org = kmalloc(sizeof(int));
		*org = origin;
		int *dest = kmalloc(sizeof(int));
		*dest = destination;
		array_add(carsOrigin, org, NULL);
		array_add(carsDest, dest, NULL);
	}
	lock_release(lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
	
		
lock_acquire(lock);
		
  for (unsigned int i = 0; i < array_num(carsOrigin); i++){
	if (*(unsigned int*)array_get(carsOrigin,i) == origin && *(unsigned int *)array_get(carsDest,i) ==  destination){
		array_remove(carsOrigin, i);
		array_remove(carsDest, i);
		break;
	}
  }

  cv_signal(cv, lock);
lock_release(lock);
}
