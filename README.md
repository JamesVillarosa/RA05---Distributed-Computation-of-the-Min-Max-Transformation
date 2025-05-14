# How to Run lab05

This document outlines the steps to compile and run the `lab05` application, which automatically creates terminals and performs min-max transformation using a master-slave process.

## Setup

1.  **Compilation:**
    * Open any terminal window.
    * Navigate to the directory containing `lab05.c`.
    * Compile and run the code using GCC with the following command:
        ```bash
        gcc -o lab05 lab05.c && ./lab05 <matrix_size> <port_number> 0
        ```
        * `<matrix_size>`: The size of the $N \times N$ matrix to be processed.
        * `<port_number>`: The base port number for communication. The master will use this port, and slaves will use subsequent ports.
        * `0`: This argument identifies the process as the master.

## Running the Application

Executing the compilation and run command above will:

1.  **Compile** the `lab05.c` file into an executable named `lab05`.
2.  **Run the master process.** The master process will then automatically:
    * Determine the number of slave processes needed based on the matrix size.
    * Open new terminal windows for each slave process.
    * Execute the `lab05` program in each slave terminal with the necessary parameters (master port, unique slave port, and slave identifier).
    * Distribute the matrix processing tasks to the slave processes.
    * Perform the min-max transformation using the master-slave architecture.

**Example:**

To run the application with a $10 \times 10$ matrix and starting the master on port 5000, use the following command:

```bash
gcc -o lab05 lab05.c && ./lab05 10 5000 0