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
- Sample Sort (Veda): This project involves the parallel implementation and evaluation of the Sample Sort algorithm, using MPI for inter-process communication. Sample Sort is a parallel sorting algorithm that divides data into partitions, sorting each partition locally, selecting a set of representative samples to determine partition boundaries, and redistributing data before performing a final merge. The algorithm will be tested on the Grace high-performance computing cluster
- Merge Sort (Ananya): Merge sort is a divide-and-conquer algorithm. It recursively splits an array into 2 halves, sorts each half, and then merges the two sorted halves to produce a final sorted array. In the parallel implementation of merge sort, the array is divided amongst many processors and each processor sorts a portion of the array concurrently.
- Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
