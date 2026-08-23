# cpu-scheduling-simulation-ds
A comprehensive CPU scheduling simulation (FCFS, SJF, Priority, Round Robin, SRTF) built in C++17 using custom Data Structures (Linked List, Queue, Min-Heap, Stack) without STL containers.# CPU Scheduling Simulation Using Custom Data Structures

A modular, standalone CPU Scheduling Simulator implemented in **C++17** for the Practical Data Structures course. This simulator models core operating system scheduling mechanisms using custom memory-managed data structures built from scratch, without utilizing STL container libraries (`<queue>`, `<stack>`, `<list>`).

---

## 📌 Features & Supported Algorithms
1. **First-Come, First-Served (FCFS):** Non-preemptive scheduling based strictly on arrival order.
2. **Shortest Job First (SJF):** Non-preemptive scheduling giving precedence to tasks with the shortest CPU burst time.
3. **Priority Scheduling:** Non-preemptive scheduling based on priority ranks (lower numeric value = higher priority).
4. **Round Robin (RR):** Preemptive scheduling using a configurable Time Quantum.
5. **Shortest Remaining Time First (SRTF) [Bonus]:** Preemptive SJF algorithm handling dynamic arrivals and context switches.

---

## 🧱 Custom Data Structures Implemented
- **`ProcessLinkedList`:** A singly linked list with `head` and `tail` pointers to manage the dynamic dataset of entered processes.
- **`IndexQueue`:** A custom pointer-based FIFO queue managing process readiness in FCFS and Round Robin.
- **`ReadyPriorityQueue` & `RemainingPriorityQueue`:** Array-based Binary Min-Heaps providing $O(\log n)$ extraction of the highest-priority/shortest-job task.
- **`ExecutionStack`:** A custom LIFO stack tracking execution segments and producing reverse execution traces.

---

## 🚀 Compilation and Execution

### Using GCC / G++ (Linux, macOS, Windows MinGW):
```bash
# Compile the source code
g++ -std=c++17 CPU_Scheduling_Project_Final_100.cpp -o scheduler

# Run the interactive program
./scheduler

# Run automated demo mode with the sample dataset
./scheduler --demo
