// Name: Villarosa, James Carl V.
// Section: CD-3L
// gcc -o lab05 lab05.c && ./lab05 10 5000 0


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>                                        // Libraries we need in lab05
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>
#include <time.h>

#define MAX_SLAVES 16
#define SLAVES 4

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

// Helper to allocate double matrix
double **allocate_double_matrix(int rows, int cols) {
    double **matrix = malloc(rows * sizeof(double *));
    for (int i = 0; i < rows; i++) {
        matrix[i] = malloc(cols * sizeof(double));
    }
    return matrix;
}

void free_double_matrix(double **matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// Change print_matrix to print doubles
void print_matrix(double **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%.2f\t", matrix[i][j]);
        }
        printf("\n");
    }
}

// Change min_max_transform_columns to output doubles in [0,1]
void min_max_transform_columns(int **X, double **T, int rows, int cols, int *col_min, int *col_max) {
    for (int j = 0; j < cols; j++) {
        double range = (col_max[j] - col_min[j]) ? (col_max[j] - col_min[j]) : 1;
        for (int i = 0; i < rows; i++) {
            T[i][j] = (X[i][j] - col_min[j]) / range;
        }
    }
}

void bind_slave_to_core(int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) 
    {
        perror("sched_setaffinity failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Slave bound to core %d\n", core_id);
    }
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
    for (int i = 0; i < SLAVES; i++) {
        char command[256];
        int slave_port = 5001 + i;
        snprintf(command, sizeof(command), "cmd.exe /c start wsl.exe ./lab05 %d %d 1", n, slave_port);
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

    // Print the initial matrix before transformation
    printf("Initial Matrix (before Min-Max Transformation):\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf("%4d ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");

    // Compute min and max for each column of the whole matrix
    int *col_min = malloc(n * sizeof(int));
    int *col_max = malloc(n * sizeof(int));
    for (int j = 0; j < n; j++) {
        col_min[j] = matrix[0][j];
        col_max[j] = matrix[0][j];
        for (int i = 1; i < n; i++) {
            if (matrix[i][j] < col_min[j]) col_min[j] = matrix[i][j];
            if (matrix[i][j] > col_max[j]) col_max[j] = matrix[i][j];
        }
    }

    int rows_per_slave = n / slave_count;                   // Dividing rows in slave
    int remaining_rows = n % slave_count;                   // Distributing the remaining row if not divisible
                                                            // by the number of slaves
    struct timeval start, end;
    gettimeofday(&start, NULL);                             // Get time start

    int slave_sockets[MAX_SLAVES];
    int rows_sent[MAX_SLAVES];

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
        rows_sent[i] = rows_to_send;
                
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

        send(sockfd, col_min, n * sizeof(int), 0);
        send(sockfd, col_max, n * sizeof(int), 0);

        free_matrix(submatrix, rows_to_send);                       // Freeing matrix
    }

    double **T = allocate_double_matrix(n, n);
    int current_row = 0;
    for (int i = 0; i < slave_count; i++) {
        int rows = rows_sent[i];
        printf("\nRows computed by slave %d (rows: %d):\n", i, rows);
        for (int r = 0; r < rows; r++) {
            recv(slave_sockets[i], T[current_row + r], n * sizeof(double), MSG_WAITALL);
            for (int c = 0; c < n; c++) {
                printf("%.2f\t", T[current_row + r][c]);
            }
            printf("\n");
        }
        printf("Received ACK from slave %d\n", i);
        current_row += rows;
        close(slave_sockets[i]);
    }

    gettimeofday(&end, NULL);                                       // Get time end
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Master Time Elapsed: %.6f seconds\n", elapsed);         // Compute and print time elapsed

    // Print the whole resulting matrix T
    printf("\nResulting Matrix T (Min-Max Transformed):\n");
    print_matrix(T, n, n);

    free_matrix(matrix, n);                                         // Freeing matrix
    free_double_matrix(T, n);
    free(col_min);
    free(col_max);
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

    // Bind slave to a specific core based on the port number
    int core_id = port - 5001; // 5001 maps to core 0, 5002 to core 1, etc.
    bind_slave_to_core(core_id);

    listen(sockfd, 1);                                              // Listening for upcoming connection
    printf("Slave listening on port %d...\n", port);

    socklen_t addrlen = sizeof(client_addr);                        // Accept incoming connection
    int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);

    int rows;
    recv(client_sock, &rows, sizeof(int), 0);                       // Receive number of rows in matrix and allocate it
    int **submatrix = allocate_matrix(rows, n);

    for (int i = 0; i < rows; i++)
    {
        recv(client_sock, submatrix[i], n * sizeof(int), 0);       // Receiving Matrix from the master
    }

    int *col_min = malloc(n * sizeof(int));
    int *col_max = malloc(n * sizeof(int));
    recv(client_sock, col_min, n * sizeof(int), MSG_WAITALL);
    recv(client_sock, col_max, n * sizeof(int), MSG_WAITALL);

    double **T = allocate_double_matrix(rows, n);

    struct timeval start, end;
    gettimeofday(&start, NULL);                                     // Get time start

    min_max_transform_columns(submatrix, T, rows, n, col_min, col_max);

    gettimeofday(&end, NULL);                                      // Get time end

    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Slave MMT Computation Time Elapsed: %.6f seconds\n", elapsed);

    // Print the rows computed by this slave
    printf("\nRows computed by this slave (rows: %d):\n", rows);
    print_matrix(T, rows, n);

    // Send transformed submatrix T back to master
    for (int i = 0; i < rows; i++)
        send(client_sock, T[i], n * sizeof(double), 0);

    free_matrix(submatrix, rows);                                   // Freeing submatrix
    free_double_matrix(T, rows);
    free(col_min);
    free(col_max);

    printf("\nPress Enter to exit this slave terminal...");
    getchar();

    close(client_sock);                                             // Closing sockets
    close(sockfd);
}

int main(int no_arguments, char *arguments[])           // Main function that is getting number of arguments and
{                                                       // the arguments when running the programs
    srand(time(NULL)); // Seed the random number generator

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
