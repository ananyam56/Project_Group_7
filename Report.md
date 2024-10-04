# CSCE 435 Group project

## 0. Group number: 7

## 1. Group members:
1. First: Ananya Maddali 
2. Second: Jordyn Hamstra
3. Third: Abigail Hunt
4. Fourth: Veda Javalagi

## 2. Project topic (e.g., parallel sorting algorithms)
Design and Analysis of Parallel Sorting Algorithms

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort (Abigail):
- Sample Sort (Veda): This project involves the parallel implementation and evaluation of the Sample Sort algorithm, using MPI for inter-process communication. Sample Sort is a parallel sorting algorithm that divides data into partitions, sorting each partition locally, selecting a set of representative samples to determine partition boundaries, and redistributing data before performing a final merge. The algorithm will be tested on the Grace high-performance computing cluster.
- Merge Sort (Ananya): Merge sort is a divide-and-conquer algorithm. It recursively splits an array into 2 halves, sorts each half, and then merges the two sorted halves to produce a final sorted array using MPI calls. In the parallel implementation of merge sort, the array is divided amongst many processors and each processor sorts a portion of the array concurrently. The algorithm will be tested on the Grace high-performance computing cluster.
- Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes
2. Sample Sort: 
Initialize MPI environment
   MPI_Init(&argc, &argv);
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);      

Generate or receive data on the root process (process 0)
   if rank == 0 then
      data = GenerateData(total_data_size);
   end if

Broadcast data size to all processes
   MPI_Bcast(&data_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

Scatter data to all processes
   local_data = Allocate buffer for (data_size / num_procs);
   MPI_Scatter(data, data_size/num_procs, MPI_TYPE, 
               local_data, data_size/num_procs, MPI_TYPE, 
               0, MPI_COMM_WORLD);

Locally sort the scattered data
   local_data = LocalSort(local_data); 

Select samples for determining partition boundaries
   samples = SelectSamples(local_data, num_procs);

Gather samples to root process
   all_samples = Allocate buffer for (num_procs * (num_procs - 1));
   MPI_Gather(samples, num_procs - 1, MPI_TYPE, 
              all_samples, num_procs - 1, MPI_TYPE, 
              0, MPI_COMM_WORLD);

Root process sorts all samples and determines partition boundaries
   if rank == 0 then
      sorted_samples = LocalSort(all_samples);
      boundaries = DeterminePartitionBoundaries(sorted_samples);
   end if

Broadcast partition boundaries to all processes
   MPI_Bcast(boundaries, num_procs - 1, MPI_TYPE, 0, MPI_COMM_WORLD);

Redistribute data according to partition boundaries
    send_counts = DetermineSendCounts(local_data, boundaries);
    recv_counts = Allocate buffer for (num_procs);
    MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, MPI_COMM_WORLD);

    send_displacements = CalculateDisplacements(send_counts);
    recv_displacements = CalculateDisplacements(recv_counts);

    final_data = Allocate buffer for total received data;
    MPI_Alltoallv(local_data, send_counts, send_displacements, MPI_TYPE,
                  final_data, recv_counts, recv_displacements, MPI_TYPE,
                  MPI_COMM_WORLD);

Locally sort the final received data
    final_data = LocalSort(final_data);

Gather sorted data to the root process (optional)
    MPI_Gather(final_data, recv_counts[rank], MPI_TYPE, 
               sorted_data, recv_counts[rank], MPI_TYPE, 
               0, MPI_COMM_WORLD);

Finalize MPI environment
    MPI_Finalize();


### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
