/**
 * @file client.c
 * @author Divyateja Pasupuleti
 * @brief
 * @version 0.1
 * @date 2023-11-04
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program client.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

#define MESSAGE_LENGTH 100
#define LOAD_BALANCER_CHANNEL 1
#define PRIMARY_SERVER_CHANNEL 2
#define SECONDARY_SERVER_CHANNEL_1 3
#define SECONDARY_SERVER_CHANNEL_2 4

struct data
{
    long client_id;
    long seq_num;
    long operation;
    char graph_name[MESSAGE_LENGTH];
};

struct msg_buffer
{
    long msg_type;
    struct data data;
};

/**
 * @brief On execution, each instance of this program creates a separate client process,
 * i.e., if the executable file corresponding to client.c is client.out, then each time
 * client.out is executed on a separate terminal, a separate client process is
 * created.
 *
 * @return int 0
 */
int main()
{
    // Initialize the client
    printf("[Client] Initializing Client...\n");

    key_t key;
    int msg_queue_id;
    struct msg_buffer message;

    // Generate key for the message queue
    while ((key = ftok(".", 'B')) == -1)
    {
        perror("[Client] Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Connect to the messsage queue
    while ((msg_queue_id = msgget(key, 0644)) == -1)
    {
        perror("[Client] Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("[Client] Successfully connected to the Message Queue %d %d\n", key, msg_queue_id);

    // When a client process is run, it will ask the user to enter a positive integer as its client-id
    int client_id;
    printf("[Client] Enter Client ID: ");
    scanf("%d", &client_id);

    if (client_id == 0)
    {
        printf("Client ID cannot be 0\n");
        exit(EXIT_FAILURE);
    }

    // This is being done to ensure that the client_id is unique and cannot be 0,1,2,3,4
    // because these are being used already
    client_id += 4;
    printf("[Client] Your assigned Client ID: %d\n", client_id);
    message.data.client_id = client_id;

    // Display the menu
    while (1)
    {
        printf("\n");
        printf("Choose from one of the options below: \n");
        printf("1. Add a new graph to the database\n");
        printf("2. Modify an existing graph of the database\n");
        printf("3. Perform DFS on an existing graph of the database\n");
        printf("4. Perform BFS on an existing graph of the database\n");
        printf("5. Exit\n");

        int seq_num;
        printf("Enter Sequence Number: ");
        scanf("%d", &seq_num);

        int operation;
        printf("Enter Operation Number: ");
        scanf("%d", &operation);

        printf("Enter Graph Name: ");
        scanf("%s", message.data.graph_name);

        printf("\nInput given: Seq: %d Op: %d Name: %s\n", seq_num, operation, message.data.graph_name);

        if (operation == 1)
        {
            // Input number of nodes
            int number_of_nodes;
            printf("Enter Number of Nodes: ");
            scanf("%d", &number_of_nodes);

            // Input adjacency matrix
            int adjacency_matrix[number_of_nodes][number_of_nodes];
            printf("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters: \n");
            for (int i = 0; i < number_of_nodes; i++)
            {
                for (int j = 0; j < number_of_nodes; j++)
                {
                    scanf("%d", &adjacency_matrix[i][j]);
                }
            }

            // Connect to shared memory
            int shm_id;
            if ((shm_id = shmget(key, sizeof(shm), 0666 | IPC_CREAT)) == -1)
            {
                printf("Error occurred while connecting to shm\n");
                return -1;
            }

            message.msg_type = LOAD_BALANCER_CHANNEL;
            message.data.operation = 1;
            message.data.seq_num = seq_num;

            if (msgsnd(msg_queue_id, &message, sizeof(message.data), 0) == -1)
            {
                perror("[Client: Op 1] Message could not be sent, please try again");
                exit(EXIT_FAILURE);
            }
        }
        else if (operation == 2)
        {
        }
        else if (operation == 3)
        {
        }
        else if (operation == 4)
        {
        }
        else if (operation == 5)
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Invalid Input. Please try again.\n");
        }
    }

    return 0;
}