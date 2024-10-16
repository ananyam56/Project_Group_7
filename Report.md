# CSCE 435 Group project

## 0. Group number: 7

## 1. Group members:
1. First: Ananya Maddali 
2. Second: Jordyn Hamstra
3. Third: Abigail Hunt
4. Fourth: Veda Javalagi

Communication Style: We plan on communicating regularly on a text group chat, and we will utilize discord collaborate when we cannot meet in person. 

## 2. Project topic (e.g., parallel sorting algorithms)
Design and Analysis of Parallel Sorting Algorithms

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort (Abigail): Bitonic sort is a parallel sorting algorithm that works by constructing a bitonic sequence - a sequence of numbers that first increases and then decreases. The sorting process involves recursively dividing a larger bitonic sequence into smaller bitonic sequences. The algorithm compares corresponding elements within these smaller sequences, swapping elements as necessary to sort them in ascending order. Finally, the algorithm merges these smaller sequences back together. The algorithm will be tested on the Grace high-performance computing cluster.
- Sample Sort (Veda): This project involves the parallel implementation and evaluation of the Sample Sort algorithm, using MPI for inter-process communication. Sample Sort is a parallel sorting algorithm that divides data into partitions, sorting each partition locally, selecting a set of representative samples to determine partition boundaries, and redistributing data before performing a final merge. The algorithm will be tested on the Grace high-performance computing cluster.
- Merge Sort (Ananya): Merge sort is a divide-and-conquer algorithm. It recursively splits an array into 2 halves, sorts each half, and then merges the two sorted halves to produce a final sorted array using MPI calls. In the parallel implementation of merge sort, the array is divided amongst many processors and each processor sorts a portion of the array concurrently. The algorithm will be tested on the Grace high-performance computing cluster.
- Radix Sort (Jordyn): Radix sort sorts an array by sorting by each digits place starting with the ones place. It will iterate through the array as many times as there are digits in the largest value. Each iteration is done in parallel and then will be combined to produce a sorted array using MPI. The algorithm will be tested on the Grace high-performance computing cluster.

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes
  
1. Bitonic Sort:
```Initialize MPI environment
MPI_Init(&argc, &argv);
MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);      

// Generate or receive data on the root process (process 0)
if rank == 0 then
    data = GenerateData(total_data_size); // Create data to sort
end if

// Broadcast data size to all processes
MPI_Bcast(&data_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

// Scatter data to all processes
MPI_Scatter(data, data_size/num_procs, MPI_INT, 
            local_data, data_size/num_procs, MPI_INT, 
            0, MPI_COMM_WORLD);

// Each process sorts its local data to create a bitonic sequence
local_data = BitonicSort(local_data);

// Bitonic merging
for i=2 to num_procs do
    j = data_size / num_procs; //Initialize j to the size of the data chunk
    while j > 0 do // Use a while loop to decrement j to 1
        // Merge with the next process based on current step
        if rank % i == 0 then
            // Prepare buffers for receiving data
            recv_data = allocate_buffer_for(data_size/num_procs);
            MPI_Sendrecv(local_data, data_size/num_procs, MPI_INT, rank + (data_size/num_procs), 0,
                         recv_data, data_size/num_procs, MPI_INT, rank - (data_size/num_procs), 0);
            // Merge local_data with recv_data
            local_data = BitonicMerge(local_data, recv_data);
        end if
        j = j/2; // Halve j to reduce the size of the data chunk for the next iteration
    end for
end for

// Gather the sorted data back to the root process
MPI_Gather(local_data, data_size/num_procs, MPI_INT, 
           sorted_data, data_size/num_procs, MPI_INT, 
           0, MPI_COMM_WORLD);

// If rank 0, output the final sorted array
if rank == 0 then
    Print(sorted_data); // Display or use sorted data
end if

// Finalize MPI environment
MPI_Finalize();

// Function to perform bitonic sort on local data
BitonicSort(data)
    if length(data) > 1 then
        mid = length(data) / 2
        BitonicSort(data[0..mid-1]); // Sort first half in ascending order
        BitonicSort(data[mid..n-1]); // Sort second half in descending order
        return data; // Combine sorted halves
    end if
    return data;

// Function to merge two sorted arrays into one
BitonicMerge(a, b)
    n = length(a) + length(b)
    for i from 0 to n/2 do
        if a[i] > b[i] then
            swap(a[i], b[i]); // Swap elements if out of order
        end if
    end for
    return Combine(a, b); // Return combined sorted array
```

2. Sample Sort: 
```Initialize MPI environment
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
    
```
3. Merge Sort:

```Initialize the MPI environment
   MPI_Init()

Get the rank (process ID) of the current process and the total number of processes
   rank = MPI_Comm_rank(MPI_COMM_WORLD)
   size = MPI_Comm_size(MPI_COMM_WORLD)

Determine the chunk of the array each process will handle
   chunk_size = n / size   // Divide the array evenly among processes

Allocate memory for the local_chunk each process will sort
   local_chunk = allocate memory for chunk_size elements

Use MPI_Scatter to distribute the data from process 0 to all other processes
   MPI_Scatter(A, chunk_size, MPI_INT, local_chunk, chunk_size, MPI_INT, 0, MPI_COMM_WORLD)

Each process performs a sequential Merge Sort on its local_chunk
   local_chunk = merge_sort(local_chunk)

Use MPI_Gather to collect the sorted chunks back to process 0
   MPI_Gather(local_chunk, chunk_size, MPI_INT, A, chunk_size, MPI_INT, 0, MPI_COMM_WORLD)

On process 0, merge the sorted chunks to obtain the final sorted array
   If rank == 0:
      A = parallel_merge(A, size)

Finalize the MPI environment
   MPI_Finalize()

// Sequential Merge Sort function (for local chunk sorting)
Function merge_sort(A):
    If length(A) <= 1:
        Return A
    Else:
        Split A into two halves: left and right
        left_sorted = merge_sort(left)
        right_sorted = merge_sort(right)
        Return merge(left_sorted, right_sorted)

// Parallel Merge function (used to merge sorted chunks on process 0)
Function parallel_merge(A, P):
    While P > 1:
        For each pair of adjacent chunks:
            Merge the pair into one sorted chunk
        P = P / 2
    Return A
```
4. Radix Sort:

```
// Initialize MPI environment
MPI_Init()
rank = MPI_Comm_rank()          // Get the rank of the process
size = MPI_Comm_size()          // Get the total number of processes

// Distribute the data among all processes
local_data = distribute_data_evenly()  // Each process is responsible for reading a chunk of the data

// Perform local radix sort on the local chunk of data
for each digit from least significant to most significant do
    count = count_digit_occurrences(local_data, digit)         // Count occurrences of each digit
    prefix_sum = calculate_prefix_sum(count)                   // Create prefix sum based on counts
    local_data = reorder_data(local_data, prefix_sum, digit)   // Reorder data based on the digit
    
    // Exchange sorted data between processes based on digit values
    for each process i do
        send_chunk = find_data_chunk_for_process(local_data, i)  // Determine the data chunk to send to process i
        recv_chunk = MPI_Alltoall(send_chunk)                    // Exchange data chunks with all processes
    end for
    
    // Merge received data
    local_data = merge_and_sort(recv_chunk)
end for

// After the final iteration each process contains part of the sorted data

// Combine the results into a single array
final_data ← MPI_Allgather(local_data)

// Finalize MPI environment
MPI_Finalize()
```

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types:
  -  input_size's: 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28
  -  input_type's: Sorted, Random, Reverse, Sorted with 1% perturbed
  -  MPI num_procs: 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
- Strong scaling (same problem size, increase number of processors/nodes)
  - we will keep the input size constant while increasing the number of processors. The goal is to see how efficiently the algorithm can utilize more processors to reduce the runtime for a fixed problem size. This helps in identifying the point where adding more processors does not significantly improve performance 
- Weak scaling (increase problem size, increase number of processors)
  - every time the problem sizes are increased, the num_procs will be increased proportionally as well. the results will be plotted with num_procs on the x-axis and input_sizes on the y-axis

### 3a. Caliper instrumentation
Please use the caliper build `/scratch/group/csce435-f24/Caliper/caliper/share/cmake/caliper` 
(same as lab2 build.sh) to collect caliper files for each experiment you run.

Your Caliper annotations should result in the following calltree
(use `Thicket.tree()` to see the calltree):
```
main
|_ data_init_X      # X = runtime OR io
|_ comm
|    |_ comm_small
|    |_ comm_large
|_ comp
|    |_ comp_small
|    |_ comp_large
|_ correctness_check
```

Required region annotations:
- `main` - top-level main function.
    - `data_init_X` - the function where input data is generated or read in from file. Use *data_init_runtime* if you are generating the data during the program, and *data_init_io* if you are reading the data from a file.
    - `correctness_check` - function for checking the correctness of the algorithm output (e.g., checking if the resulting data is sorted).
    - `comm` - All communication-related functions in your algorithm should be nested under the `comm` region.
      - Inside the `comm` region, you should create regions to indicate how much data you are communicating (i.e., `comm_small` if you are sending or broadcasting a few values, `comm_large` if you are sending all of your local values).
      - Notice that auxillary functions like MPI_init are not under here.
    - `comp` - All computation functions within your algorithm should be nested under the `comp` region.
      - Inside the `comp` region, you should create regions to indicate how much data you are computing on (i.e., `comp_small` if you are sorting a few values like the splitters, `comp_large` if you are sorting values in the array).
      - Notice that auxillary functions like data_init are not under here.
    - `MPI_X` - You will also see MPI regions in the calltree if using the appropriate MPI profiling configuration (see **Builds/**). Examples shown below.

All functions will be called from `main` and most will be grouped under either `comm` or `comp` regions, representing communication and computation, respectively. You should be timing as many significant functions in your code as possible. **Do not** time print statements or other insignificant operations that may skew the performance measurements.

### **Nesting Code Regions Example** - all computation code regions should be nested in the "comp" parent code region as following:
```
CALI_MARK_BEGIN("comp");
CALI_MARK_BEGIN("comp_small");
sort_pivots(pivot_arr);
CALI_MARK_END("comp_small");
CALI_MARK_END("comp");

# Other non-computation code
...

CALI_MARK_BEGIN("comp");
CALI_MARK_BEGIN("comp_large");
sort_values(arr);
CALI_MARK_END("comp_large");
CALI_MARK_END("comp");
```

### **Calltree Example**:
```
# MPI Mergesort
4.695 main
├─ 0.001 MPI_Comm_dup
├─ 0.000 MPI_Finalize
├─ 0.000 MPI_Finalized
├─ 0.000 MPI_Init
├─ 0.000 MPI_Initialized
├─ 2.599 comm
│  ├─ 2.572 MPI_Barrier
│  └─ 0.027 comm_large
│     ├─ 0.011 MPI_Gather
│     └─ 0.016 MPI_Scatter
├─ 0.910 comp
│  └─ 0.909 comp_large
├─ 0.201 data_init_runtime
└─ 0.440 correctness_check
```

### **Calltrees for Each Algorithm**:
```
# MPI Bitonic Sort
0.540 main
├─ 0.039 MPI_Comm_dup
├─ 0.000 MPI_Finalize
├─ 0.000 MPI_Finalized
├─ 0.000 MPI_Init
├─ 0.000 MPI_Initialized
├─ 0.010 comm
│  ├─ 0.003 comm_large
│  │  ├─ 0.000 MPI_Gather
│  │  └─ 0.002 MPI_Scatter
│  └─ 0.008 comm_small
│     └─ 0.007 MPI_Sendrecv
├─ 0.011 comp
│  └─ 0.011 comp_large
├─ 0.000 correctness_check
└─ 0.000 data_init_runtime

1.0917 main
├─ 0.0000 MPI_Init
├─ 0.0024 MPI_Bcast
├─ 0.0668 data_init_runtime
├─ 0.0604 comm
│  ├─ 0.0586 MPI_Comm_dup
│  ├─ 0.0002 MPI_Barrier
│  └─ 0.0015 comm_large
│     ├─ 0.0012 MPI_Scatter
│     └─ 0.0003 MPI_Gather
├─ 0.3526 comp
│  └─ 0.3526 comp_large
├─ 0.0000 MPI_Finalize
├─ 0.0143 correctness_check
├─ 0.0000 MPI_Initialized
├─ 0.0000 MPI_Finalized
└─ 0.1593 MPI_Comm_dup
```


### 3b. Collect Metadata

Have the following code in your programs to collect metadata:
```
adiak::init(NULL);
adiak::launchdate();    // launch date of the job
adiak::libraries();     // Libraries used
adiak::cmdline();       // Command line used to launch the job
adiak::clustername();   // Name of the cluster
adiak::value("algorithm", algorithm); // The name of the algorithm you are using (e.g., "merge", "bitonic")
adiak::value("programming_model", programming_model); // e.g. "mpi"
adiak::value("data_type", data_type); // The datatype of input elements (e.g., double, int, float)
adiak::value("size_of_data_type", size_of_data_type); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
adiak::value("input_size", input_size); // The number of elements in input dataset (1000)
adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
adiak::value("scalability", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
adiak::value("group_num", group_number); // The number of your group (integer, e.g., 1, 10)
adiak::value("implementation_source", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
```

They will show up in the `Thicket.metadata` if the caliper file is read into Thicket.

### **See the `Builds/` directory to find the correct Caliper configurations to get the performance metrics.** They will show up in the `Thicket.dataframe` when the Caliper file is read into Thicket.
## 4. Performance evaluation

Include detailed analysis of computation performance, communication performance. 
Include figures and explanation of your analysis.

### 4a. Vary the following parameters
For input_size's:
- 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28

For input_type's:
- Sorted, Random, Reverse sorted, 1%perturbed

MPI: num_procs:
- 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024

This should result in 4x7x10=280 Caliper files for your MPI experiments.

### 4b. Hints for performance analysis

To automate running a set of experiments, parameterize your program.

- input_type: "Sorted" could generate a sorted input to pass into your algorithms
- algorithm: You can have a switch statement that calls the different algorithms and sets the Adiak variables accordingly
- num_procs: How many MPI ranks you are using

When your program works with these parameters, you can write a shell script 
that will run a for loop over the parameters above (e.g., on 64 processors, 
perform runs that invoke algorithm2 for Sorted, ReverseSorted, and Random data).  

### 4c. You should measure the following performance metrics
- `Time`
    - Min time/rank
    - Max time/rank
    - Avg time/rank
    - Total time
    - Variance time/rank


## 5. Presentation
Plots for the presentation should be as follows:
- For each implementation:
    - For each of comp_large, comm, and main:
        - Strong scaling plots for each input_size with lines for input_type (7 plots - 4 lines each)
        - Strong scaling speedup plot for each input_type (4 plots)
        - Weak scaling plots for each input_type (4 plots)

Analyze these plots and choose a subset to present and explain in your presentation.

## 6. Final Report
Submit a zip named `TeamX.zip` where `X` is your team number. The zip should contain the following files:
- Algorithms: Directory of source code of your algorithms.
- Data: All `.cali` files used to generate the plots seperated by algorithm/implementation.
- Jupyter notebook: The Jupyter notebook(s) used to generate the plots for the report.
- Report.md
