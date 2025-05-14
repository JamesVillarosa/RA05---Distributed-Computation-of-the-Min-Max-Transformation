// Name: Villarosa, James Carl V.
// Section: CD-3L

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>                                        // Libraries we need in lab04
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>

#define MAX_SLAVES 16
#define print_error_then_terminate(en, msg) \
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

int **allocate_matrix(int rows, int cols)                   // Function for allocation memory in matrix
{
    int **matrix = malloc(rows * sizeof(int *));            // Allocating memory in matrix

    for (int i = 0; i < rows; i++) 
    {
        matrix[i] = malloc(cols * sizeof(int));             // Allocating memory based on user input "n"
    }

    return matrix;
}

void free_matrix(int **matrix, int rows)                    // Function for freeing matrix
{
    for (int i = 0; i < rows; i++) 
    {
        free(matrix[i]);                                    // Freeing every element in matrix
    }
    free(matrix);                                           // Freeing matrix
}

void run_master(int n, int port) 
{
    FILE *fp = fopen("config.txt", "r");                    // Opening config.txt
    if (!fp) 
    {
        printf("Cannot open config.txt\n");                 // If file not existing, prompt error to user
        exit(EXIT_FAILURE);
    }

    // Automatically spawn slave terminals
    for (int i = 0; i < 8; i++) {
        char command[256];
        int slave_port = 5001 + i;
        snprintf(command, sizeof(command), "cmd.exe /c start wsl.exe ./lab04 %d %d 1", n, slave_port);
        system(command);
        sleep(2);
    }

    sleep(2); // Give slaves time to start

    char ip_addresses[MAX_SLAVES][32];
    int ports[MAX_SLAVES];                                  // Variables need in master function
    int slave_count = 0;

    while (fscanf(fp, "%s %d", ip_addresses[slave_count], &ports[slave_count]) == 2) 
    {
        slave_count++;                                      // Counting valid slave in valid format in config.txt
    }
    fclose(fp);

    int **matrix = allocate_matrix(n, n);                   // Call allocate matrix function

    for (int i = 0; i < n; i++)                             // Filling up matrix with random numbers
    {
        for (int j = 0; j < n; j++)
        {
            matrix[i][j] = 1 + rand() % 100;
        }
    }

    int rows_per_slave = n / slave_count;                   // Dividing rows in slave
    int remaining_rows = n % slave_count;                   // Distributing the remaining row if not divisible
                                                            // by the number of slaves
    struct timeval start, end;
    gettimeofday(&start, NULL);                             // Get time start

    int slave_sockets[MAX_SLAVES];

    for (int i = 0; i < slave_count; i++) 
    {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);               // Creating socket

        struct sockaddr_in slave_addr;                              // Preparing slave's info
        slave_addr.sin_family = AF_INET;

        slave_addr.sin_port = htons(ports[i]);                          // Setting port number and IP address
        inet_pton(AF_INET, ip_addresses[i], &slave_addr.sin_addr);

        printf("Connecting to slave %d at %s:%d\n", i, ip_addresses[i], ports[i]);
        if (connect(sockfd, (struct sockaddr *)&slave_addr, sizeof(slave_addr)) < 0) // Connecting to slave
        {
            printf("Connect failed");                           // If connect < 0, connection failed
            continue;
        }

        slave_sockets[i] = sockfd;                                  // Storing socket

        int rows_to_send = rows_per_slave;
        if (i < remaining_rows)                                     // Determining rows to send
        {
            rows_to_send += 1;
        }
                
        int **submatrix = allocate_matrix(rows_to_send, n);         // Allocate memory for submatrix

        int offset;
        if (i < remaining_rows) 
        {
            offset = i * (rows_per_slave + 1);                      // Slave gets one of the extra rows
        } 
        else 
        {
            offset = i * rows_per_slave + remaining_rows;           // Slaves don't get an extra row
        }
        
        for (int r = 0; r < rows_to_send; r++) 
        {
            for (int c = 0; c < n; c++) 
            {
                submatrix[r][c] = matrix[offset + r][c];            // Copying the matrix into submatrix
            }
        }        

        send(sockfd, &rows_to_send, sizeof(int), 0);                // Send number of rows to slave

        for (int i = 0; i < rows_to_send; i++)
        {
            send(sockfd, submatrix[i], n * sizeof(int), 0);         // Sending Matrix to slave
        }

        free_matrix(submatrix, rows_to_send);                       // Freeing matrix
    }

    for (int i = 0; i < slave_count; i++)                           // Wait for all ACKs from all slaves
    {
        char ack[4] = {0};
        recv(slave_sockets[i], ack, sizeof(ack), 0);
        printf("Received ACK from slave %d\n", i);
        close(slave_sockets[i]);
    }

    gettimeofday(&end, NULL);                                       // Get time end
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Master Time Elapsed: %.6f seconds\n", elapsed);         // Compute and print time elapsed

    free_matrix(matrix, n);                                         // Freeing matrix
}

void run_slave(int n, int port) 
{
    FILE *fp = fopen("config.txt", "r");                            // Opening file config.txt
    if (!fp) 
    {
        printf("Cannot open config.txt");                           // If config.txt not existing, prompt user
        exit(EXIT_FAILURE);
    }

    char master_ip[32];                                             // Initializing master's ip and port number
    int port_number;
    fscanf(fp, "%s %d", master_ip, &port_number);                   // Containing masters ip(1st parameter in config.txt)
    fclose(fp);                                                     // and port number (2nd parameter in config.txt)

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);                   // Creating socket

    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;                               // Preparing the server address
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)     // Binding socket to address
    {
        printf("Socket Bind Failed\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket Successfully Binded\n"); 
    }

    listen(sockfd, 1);                                              // Listening for upcoming connection
    printf("Slave listening on port %d...\n", port);

    socklen_t addrlen = sizeof(client_addr);                        // Accept incoming connection
    int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);

    int rows;
    recv(client_sock, &rows, sizeof(int), 0);                       // Receive number of rows in matrix and allocate it
    int **submatrix = allocate_matrix(rows, n);

    struct timeval start, end;
    gettimeofday(&start, NULL);                                     // Get time start

    for (int i = 0; i < rows; i++)
    {
        recv(client_sock, submatrix[i], n * sizeof(int), 0);       // Receiving Matrix from the master
    }

    gettimeofday(&end, NULL);                                      // Get time end

    printf("Slave received submatrix (%d x %d):\n", rows, n);       // Print received submatrix for verification

    // for (int i = 0; i < rows; i++) 
    // {
    //     for (int j = 0; j < n; j++)                            // If we want to verify matrix received by slave, print
    //     {
    //         printf("%3d ", submatrix[i][j]);
    //     }
    //     printf("\n");
    // }

    char ack[] = "ack";
    send(client_sock, ack, strlen(ack), 0);                         // Send acknowledgement to master

    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Slave Time Elapsed: %.6f seconds\n", elapsed);          // Compute for time elapse

    free_matrix(submatrix, rows);                                   // Freeing submatrix

    close(client_sock);                                             // Closing sockets
    close(sockfd);
}

int main(int no_arguments, char *arguments[])           // Main function that is getting number of arguments and
{                                                       // the arguments when running the programs
    if (no_arguments != 4) 
    {
        fprintf(stderr, "Usage: %s <matrix_size> <port> <status>\n", arguments[0]);     // Arguments typed must be 4 including the file name 
        exit(EXIT_FAILURE);
    }

    int n = atoi(arguments[1]);
    int port = atoi(arguments[2]);                      // Containing arguments as variable n, port, and status
    int status = atoi(arguments[3]);

    if (status == 0)
    {
        run_master(n, port);                            // If status is master, call run master function
    }
    else
    {
        run_slave(n, port);                             // If status if slave, call run slave function
    }

    return 0;
}
