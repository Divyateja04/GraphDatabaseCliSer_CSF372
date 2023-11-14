#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>

#define MESSAGE_LENGTH 100
#define LOAD_BALANCER_CHANNEL 4000
#define PRIMARY_SERVER_CHANNEL 4001
#define SECONDARY_SERVER_CHANNEL_1 4002
#define SECONDARY_SERVER_CHANNEL_2 4003
#define MAX_THREADS 200
#define MAX_QUEUE_SIZE 100
#define MAX_NODES 100

struct data
{
    long seq_num;
    long operation;
    char graph_name[MESSAGE_LENGTH];
};

struct msg_buffer
{
    long msg_type;
    struct data data;
    int bfs_result[MAX_NODES];
};

struct data_to_thread
{
    int msg_queue_id;
    struct msg_buffer msg;
    int current_vertex;
    int number_of_nodes;
    int **adjacency_matrix;
    int *visited;
    int bfs_result[MAX_NODES];
    int bfs_result_index;
    
};

// Code for queue

// Queue structure
struct Queue
{
    int items[MAX_QUEUE_SIZE];
    int front;
    int rear;
};

// Function to create an empty queue
struct Queue *createQueue()
{
    struct Queue *queue = (struct Queue *)malloc(sizeof(struct Queue));
    queue->front = -1;
    queue->rear = -1;
    return queue;
}

// Function to check if the queue is empty
int isEmpty(struct Queue *queue)
{
    return queue->front == -1;
}

// Function to check if the queue is full
int isFull(struct Queue *queue)
{
    return queue->rear == MAX_QUEUE_SIZE - 1;
}

// Function to enqueue an item to the queue
void enqueue(struct Queue *queue, int value)
{
    if (isFull(queue))
    {
        printf("Queue is full. Cannot enqueue %d.\n", value);
        return;
    }

    if (isEmpty(queue))
    {
        queue->front = 0;
    }

    queue->rear++;
    queue->items[queue->rear] = value;
    printf("Enqueued %d to the queue.\n", value);
}

// Function to dequeue an item from the queue
int dequeue(struct Queue *queue)
{
    int item;

    if (isEmpty(queue))
    {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }

    item = queue->items[queue->front];
    queue->front++;

    if (queue->front > queue->rear)
    {
        // Reset the queue after all elements are dequeued
        queue->front = queue->rear = -1;
    }

    printf("Dequeued %d from the queue.\n", item);
    return item;
}

// Function to get the front item of the queue without removing it
int front(struct Queue *queue)
{
    if (isEmpty(queue))
    {
        printf("Queue is empty.\n");
        return -1;
    }

    return queue->items[queue->front];
}

// Function to print the elements of the queue
void display(struct Queue *queue)
{
    int i;
    if (isEmpty(queue))
    {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue elements: ");
    for (i = queue->front; i <= queue->rear; i++)
    {
        printf("%d ", queue->items[i]);
    }
    printf("\n");
}

void *bfs_subthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;
    struct Queue *queue = createQueue();
    enqueue(queue, dtt->current_vertex);
    dtt->visited[dtt->current_vertex] = 1;
    while (!isEmpty(queue))
    {
    	int currentVertex= dequeue(queue);
    	dtt->bfs_result[dtt->bfs_result_index++] = currentVertex; //Record the order of traversal
    	//printf("[Secondary Server] BFS Thread: Current vertex: %d\n", currentVertex);
	for (int i = 0; i < dtt->number_of_nodes; i++)
        {
            if (dtt->adjacency_matrix[currentVertex][i] == 1 && dtt->visited[i] == 0)
            {
                enqueue(queue, i);
                dtt->visited[i] = 1;
            }
        }
    }
    pthread_exit(NULL);
}

void *bfs_mainthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;
    // Connect to shared memory to retrieve the starting vertex
    key_t shm_key;
    int shm_id;
    while ((shm_key = ftok(".", dtt->msg.data.seq_num)) == -1)
    {
        perror("[Secondary Server] Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Generated shared memory key %d\n", shm_key);
    // Connect to the shared memory using the key
    if ((shm_id = shmget(shm_key, sizeof(dtt->number_of_nodes), 0666)) == -1)
    {
        perror("[Secondary Server] Error occurred while connecting to shm\n");
        exit(EXIT_FAILURE);
    }
    // Attach to the shared memory
    int *shmptr = (int *)shmat(shm_id, NULL, 0);
    if (shmptr == (void *)-1)
    {
        perror("[Secondary Server] Error in shmat \n");
        exit(EXIT_FAILURE);
    }

    dtt->current_vertex = *shmptr;

    FILE *fp;
    char filename[250];
    snprintf(filename, sizeof(filename), "%s", dtt->msg.data.graph_name);
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[Secondary Server] Error while opening the file");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "%d", &(dtt->number_of_nodes));

    dtt->adjacency_matrix = (int **)malloc(dtt->number_of_nodes * sizeof(int *));
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        dtt->adjacency_matrix[i] = (int *)malloc(dtt->number_of_nodes * sizeof(int));
    }
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        for (int j = 0; j < dtt->number_of_nodes; j++)
        {
            fscanf(fp, "%d ", &(dtt->adjacency_matrix[i][j]));
        }
        fclose(fp);
        //A few checks
        dtt->visited = (int *)malloc(dtt->number_of_nodes * sizeof(int));
    	dtt->visited[dtt->current_vertex] = 1;
    	int startingNode = dtt->current_vertex + 1;

    	printf("[Secondary Server] BFS Request: Adjacency Matrix Read Successfully\n");
   	printf("[Secondary Server] BFS Request: Number of nodes: %d\n", dtt->number_of_nodes);
    	printf("[Secondary Server] BFS Request: Starting vertex: %d\n", startingNode);

	pthread_t bfs_thread_id[dtt->number_of_nodes];
    	if (pthread_create(&bfs_thread_id[dtt->current_vertex], NULL, bfsThread, (void *)dtt) != 0)
    	{
        	perror("[Secondary Server] Error in BFS thread creation");
        	exit(EXIT_FAILURE);
    	}
    	// Join all BFS threads
	for (int i = 0; i < dtt->number_of_nodes; i++)
	{
    		pthread_join(bfs_thread_id[i], NULL);
	}

    	
    	//Sending result back to client
    	dtt->msg.msg_type = dtt->msg.data.seq_num;
    	dtt->msg.data.operation = 0;
    	
    	printf("[Secondary Server] Sending reply to the client %ld @ %d\n", dtt->msg.msg_type, dtt->msg_queue_id);
    	for (int i = 0; i < dtt->bfs_result_index; i++)
    	{
        	dtt->msg.bfs_result[i] = dtt->bfs_result[i];
    	}

    	if (msgsnd(dtt->msg_queue_id, &(dtt->msg), sizeof(dtt->msg), 0) == -1)
    	{
        	perror("[Secondary Server] Result of BFS could not be sent, please try again");
        	exit(EXIT_FAILURE);
    	}
    	// Detach from the shared memory
    	if (shmdt(shmptr) == -1)
    	{
        	perror("[Secondary Server] Could not detach from shared memory\n");
        	exit(EXIT_FAILURE);
    	}
    	printf("[Secondary Server] Successfully Completed Operation 1\n");
    	pthread_exit(NULL);

}

int main()
{
    // Initialize the server
    printf("[Secondary Server] Initializing Secondary Server...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok(".", 'B')) == -1)
    {
        perror("[Secondary Server] Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644)) == -1)
    {
        perror("[Secondary Server] Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Successfully connected to the Message Queue with Key:%d ID:%d\n", key, msg_queue_id);

    // Store the thread_ids thread
    pthread_t thread_ids[200];

    int channel;
    if (msg.data.seq_num % 2 == 1)
    {
        channel = SECONDARY_SERVER_CHANNEL_1;
    }
    else
    {
        channel = SECONDARY_SERVER_CHANNEL_2;
    }
    printf("[Secondary Server] Using Channel: %d\n", channel);

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        struct data_to_thread *dtt = (struct data_to_thread *)malloc(sizeof(struct data_to_thread)); // Declare dtt here

        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), channel, 0) == -1)
        {
            perror("[Secondary Server] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("[Secondary Server] Received a message from Client: Op: %ld File Name: %s\n", msg.data.operation, msg.data.graph_name);

            if (msg.data.operation == 4)
            {
                // Operation code for BFS request
                // Create a data_to_thread structure
                dtt->msg_queue_id = msg_queue_id;
                dtt->msg = msg;

                // Determine the channel based on seq_num
                int channel;
                if (msg.data.seq_num % 2 == 1)
                {
                    channel = SECONDARY_SERVER_CHANNEL_1;
                }
                else
                {
                    channel = SECONDARY_SERVER_CHANNEL_2;
                }

                // Set the channel in the message structure
                dtt->msg.msg_type = channel;
                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, bfs_mainthread, (void *)dtt) != 0)
                {
                    perror("[Secondary Server] Error in BFS thread creation");
                    exit(EXIT_FAILURE);
                }
            }
            /*
            else if (msg.data.operation == 4)
            {
            }
            */
            else if (msg.data.operation == 5)
            {
                // Operation code for cleanup
                for (int i = 0; i < 200; i++)
                {
                    pthread_join(thread_ids[i], NULL);
                }
            }
        }
    }

    return 0;
}