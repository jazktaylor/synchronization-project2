#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "BENSCHILLIBOWL.h"

// Feel free to play with these numbers! This is a great way to
// test your implementation.
#define BENSCHILLIBOWL_SIZE 100
#define NUM_CUSTOMERS 90
#define NUM_COOKS 10
#define ORDERS_PER_CUSTOMER 3
#define EXPECTED_NUM_ORDERS NUM_CUSTOMERS * ORDERS_PER_CUSTOMER

// Global variable for the restaurant.
BENSCHILLIBOWL *bcb;

/**
 * Thread funtion that represents a customer. A customer should:
 *  - allocate space (memory) for an order.
 *  - select a menu item.
 *  - populate the order with their menu item and their customer ID.
 *  - add their order to the restaurant.
 */
void* BENSCHILLIBOWLCustomer(void* tid) {
    int customer_id = (int)(long) tid;

    for (int i = 0; i < ORDERS_PER_CUSTOMER; i++) {
      Order *order = malloc(sizeof(Order));

      if (order == NULL) {
        perror("Failed to allocate order");
        return NULL;
      }

      order->menu_item = PickRandomMenuItem();
      order->customer_id = customer_id;
      order->order_number = 0;
      order->next = NULL;

      AddOrder(bcb, order);
    }

	return NULL;
}

/**
 * Thread function that represents a cook in the restaurant. A cook should:
 *  - get an order from the restaurant.
 *  - if the order is valid, it should fulfill the order, and then
 *    free the space taken by the order.
 * The cook should take orders from the restaurants until it does not
 * receive an order.
 */
void* BENSCHILLIBOWLCook(void* tid) {
    int cook_id = (int)(long) tid;
    int orders_fulfilled = 0;

    while (1) {
      Order *order = GetOrder(bcb);

      if (order == NULL) {
        printf("No more orders\n");
        break;
      }

      printf("Cook #%d is preparing order #%d: %s for customer #%d\n", cook_id, order->order_number, order->menu_item, order->customer_id);

      orders_fulfilled++;

      free(order);
    }
	  
    printf("Cook #%d fulfilled %d orders\n", cook_id, orders_fulfilled);
	  
    return NULL;
}

/**
 * Runs when the program begins executing. This program should:
 *  - open the restaurant
 *  - create customers and cooks
 *  - wait for all customers and cooks to be done
 *  - close the restaurant.
 */
int main() {
    pthread_t customers[NUM_CUSTOMERS];
    pthread_t cooks[NUM_COOKS];

    srand(time(NULL));

    bcb = OpenRestaurant(BENSCHILLIBOWL_SIZE, EXPECTED_NUM_ORDERS);

    if (bcb == NULL) {
      fprintf(stderr, "Failed to open restaurant.\n");
      return 1;
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
      pthread_create(&customers[i], NULL, BENSCHILLIBOWLCustomer, (void *)(long)i);
    }

    for (int i = 0; i < NUM_COOKS; i++) {
      pthread_create(&cooks[i], NULL, BENSCHILLIBOWLCook, (void *)(long)i);
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
      pthread_join(customers[i], NULL);
    }

    for (int i = 0; i < NUM_COOKS; i++) {
      pthread_join(cooks[i], NULL);
    }

    CloseRestaurant(bcb);

    return 0;
}