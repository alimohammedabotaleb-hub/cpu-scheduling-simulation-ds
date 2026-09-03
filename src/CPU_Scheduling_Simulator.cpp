#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

const int MAX_PROCESSES = 20;
const int MAX_SEGMENTS = 1000;

struct Process {
    string pid;
    int arrivalTime;
    int burstTime;
    int priority;
};

struct ProcessMetrics {
    int completionTime = 0;
    int waitingTime = 0;
    int turnaroundTime = 0;
};

struct GanttSegment {
    string label;
    int startTime;
    int endTime;
};

struct ScheduleResult {
    string algorithm;
    ProcessMetrics metrics[MAX_PROCESSES];
    GanttSegment gantt[MAX_SEGMENTS];
    int segmentCount = 0;
    double averageWaitingTime = 0.0;
    double averageTurnaroundTime = 0.0;
    int busyTime = 0;
    int totalTime = 0;
    double cpuUtilization = 0.0;
};

// A singly linked list stores the original process records.
class ProcessLinkedList {
private:
    struct Node {
        Process process;
        Node* next;

        explicit Node(const Process& value) : process(value), next(nullptr) {}
    };

    Node* head = nullptr;
    Node* tail = nullptr;
    int length = 0;

public:
    ProcessLinkedList() = default;
    ProcessLinkedList(const ProcessLinkedList&) = delete;
    ProcessLinkedList& operator=(const ProcessLinkedList&) = delete;

    ~ProcessLinkedList() {
        while (head != nullptr) {
            Node* oldHead = head;
            head = head->next;
            delete oldHead;
        }
    }

    void append(const Process& process) {
        Node* newNode = new Node(process);
        if (head == nullptr) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        ++length;
    }

    int copyToArray(Process destination[], int capacity) const {
        if (length > capacity) {
            throw runtime_error("The process array is too small.");
        }

        int index = 0;
        for (Node* current = head; current != nullptr; current = current->next) {
            destination[index++] = current->process;
        }
        return index;
    }
};

// A fixed-size circular queue is used by FCFS and Round Robin.
class CircularQueue {
private:
    int values[MAX_PROCESSES]{};
    int frontIndex = 0;
    int rearIndex = 0;
    int count = 0;

public:
    bool empty() const {
        return count == 0;
    }

    void enqueue(int value) {
        if (count == MAX_PROCESSES) {
            throw runtime_error("Circular queue overflow.");
        }
        values[rearIndex] = value;
        rearIndex = (rearIndex + 1) % MAX_PROCESSES;
        ++count;
    }

    int dequeue() {
        if (empty()) {
            throw runtime_error("Circular queue underflow.");
        }
        int value = values[frontIndex];
        frontIndex = (frontIndex + 1) % MAX_PROCESSES;
        --count;
        return value;
    }
};

struct HeapItem {
    int processIndex;
    int firstKey;
    int secondKey;
    int thirdKey;
};

// A binary min-heap implements the priority queue used by SJF and Priority.
class MinHeapPriorityQueue {
private:
    HeapItem heap[MAX_PROCESSES]{};
    int heapSize = 0;

    static bool comesBefore(const HeapItem& left, const HeapItem& right) {
        if (left.firstKey != right.firstKey) {
            return left.firstKey < right.firstKey;
        }
        if (left.secondKey != right.secondKey) {
            return left.secondKey < right.secondKey;
        }
        return left.thirdKey < right.thirdKey;
    }

public:
    bool empty() const {
        return heapSize == 0;
    }

    void push(const HeapItem& item) {
        if (heapSize == MAX_PROCESSES) {
            throw runtime_error("Priority queue overflow.");
        }

        int child = heapSize++;
        heap[child] = item;
        while (child > 0) {
            int parent = (child - 1) / 2;
            if (!comesBefore(heap[child], heap[parent])) {
                break;
            }
            swap(heap[child], heap[parent]);
            child = parent;
        }
    }

    HeapItem pop() {
        if (empty()) {
            throw runtime_error("Priority queue underflow.");
        }

        HeapItem minimum = heap[0];
        heap[0] = heap[--heapSize];

        int parent = 0;
        while (true) {
            int leftChild = 2 * parent + 1;
            int rightChild = 2 * parent + 2;
            int smallest = parent;

            if (leftChild < heapSize && comesBefore(heap[leftChild], heap[smallest])) {
                smallest = leftChild;
            }
            if (rightChild < heapSize && comesBefore(heap[rightChild], heap[smallest])) {
                smallest = rightChild;
            }
            if (smallest == parent) {
                break;
            }

            swap(heap[parent], heap[smallest]);
            parent = smallest;
        }

        return minimum;
    }
};

// The stack is used to show the execution history from latest to earliest.
class SegmentStack {
private:
    GanttSegment values[MAX_SEGMENTS];
    int topIndex = -1;

public:
    bool empty() const {
        return topIndex == -1;
    }

    void push(const GanttSegment& segment) {
        if (topIndex + 1 == MAX_SEGMENTS) {
            throw runtime_error("Stack overflow.");
        }
        values[++topIndex] = segment;
    }

    GanttSegment pop() {
        if (empty()) {
            throw runtime_error("Stack underflow.");
        }
        return values[topIndex--];
    }
};

void addSegment(ScheduleResult& result, const string& label, int start, int end) {
    if (start == end) {
        return;
    }
    if (result.segmentCount == MAX_SEGMENTS) {
        throw runtime_error("Too many Gantt chart segments.");
    }
    result.gantt[result.segmentCount++] = {label, start, end};
}

void buildArrivalOrder(const Process processes[], int processCount, int order[]) {
    for (int i = 0; i < processCount; ++i) {
        order[i] = i;
    }

    // Stable selection by arrival time, then by the original array position.
    for (int i = 0; i < processCount - 1; ++i) {
        int earliest = i;
        for (int j = i + 1; j < processCount; ++j) {
            int left = order[j];
            int right = order[earliest];
            if (processes[left].arrivalTime < processes[right].arrivalTime ||
                (processes[left].arrivalTime == processes[right].arrivalTime && left < right)) {
                earliest = j;
            }
        }
        swap(order[i], order[earliest]);
    }
}

void calculateFinalStatistics(ScheduleResult& result,
                              const Process processes[],
                              int processCount,
                              int finishTime) {
    double totalWaiting = 0.0;
    double totalTurnaround = 0.0;
    result.busyTime = 0;

    for (int i = 0; i < processCount; ++i) {
        result.metrics[i].turnaroundTime =
            result.metrics[i].completionTime - processes[i].arrivalTime;
        result.metrics[i].waitingTime =
            result.metrics[i].turnaroundTime - processes[i].burstTime;

        totalWaiting += result.metrics[i].waitingTime;
        totalTurnaround += result.metrics[i].turnaroundTime;
        result.busyTime += processes[i].burstTime;
    }

    result.averageWaitingTime = totalWaiting / processCount;
    result.averageTurnaroundTime = totalTurnaround / processCount;
    result.totalTime = finishTime;  // Simulation begins at time 0.
    result.cpuUtilization =
        (result.totalTime == 0)
            ? 0.0
            : (100.0 * result.busyTime / result.totalTime);
}

ScheduleResult runFCFS(const Process processes[], int processCount) {
    ScheduleResult result;
    result.algorithm = "FCFS";

    int arrivalOrder[MAX_PROCESSES]{};
    buildArrivalOrder(processes, processCount, arrivalOrder);

    CircularQueue readyQueue;
    int nextArrival = 0;
    int completed = 0;
    int currentTime = 0;

    while (completed < processCount) {
        while (nextArrival < processCount &&
               processes[arrivalOrder[nextArrival]].arrivalTime <= currentTime) {
            readyQueue.enqueue(arrivalOrder[nextArrival++]);
        }

        if (readyQueue.empty()) {
            int nextTime = processes[arrivalOrder[nextArrival]].arrivalTime;
            addSegment(result, "IDLE", currentTime, nextTime);
            currentTime = nextTime;
            continue;
        }

        int index = readyQueue.dequeue();
        int startTime = currentTime;
        currentTime += processes[index].burstTime;
        addSegment(result, processes[index].pid, startTime, currentTime);
        result.metrics[index].completionTime = currentTime;
        ++completed;
    }

    calculateFinalStatistics(result, processes, processCount, currentTime);
    return result;
}

ScheduleResult runSJF(const Process processes[], int processCount) {
    ScheduleResult result;
    result.algorithm = "SJF (Non-Preemptive)";

    int arrivalOrder[MAX_PROCESSES]{};
    buildArrivalOrder(processes, processCount, arrivalOrder);

    MinHeapPriorityQueue readyQueue;
    int nextArrival = 0;
    int completed = 0;
    int currentTime = 0;

    while (completed < processCount) {
        while (nextArrival < processCount &&
               processes[arrivalOrder[nextArrival]].arrivalTime <= currentTime) {
            int index = arrivalOrder[nextArrival++];
            readyQueue.push({index,
                             processes[index].burstTime,
                             processes[index].arrivalTime,
                             index});
        }

        if (readyQueue.empty()) {
            int nextTime = processes[arrivalOrder[nextArrival]].arrivalTime;
            addSegment(result, "IDLE", currentTime, nextTime);
            currentTime = nextTime;
            continue;
        }

        int index = readyQueue.pop().processIndex;
        int startTime = currentTime;
        currentTime += processes[index].burstTime;
        addSegment(result, processes[index].pid, startTime, currentTime);
        result.metrics[index].completionTime = currentTime;
        ++completed;
    }

    calculateFinalStatistics(result, processes, processCount, currentTime);
    return result;
}

ScheduleResult runPriority(const Process processes[], int processCount) {
    ScheduleResult result;
    result.algorithm = "Priority (Non-Preemptive)";

    int arrivalOrder[MAX_PROCESSES]{};
    buildArrivalOrder(processes, processCount, arrivalOrder);

    MinHeapPriorityQueue readyQueue;
    int nextArrival = 0;
    int completed = 0;
    int currentTime = 0;

    while (completed < processCount) {
        while (nextArrival < processCount &&
               processes[arrivalOrder[nextArrival]].arrivalTime <= currentTime) {
            int index = arrivalOrder[nextArrival++];
            readyQueue.push({index,
                             processes[index].priority,
                             processes[index].arrivalTime,
                             index});
        }

        if (readyQueue.empty()) {
            int nextTime = processes[arrivalOrder[nextArrival]].arrivalTime;
            addSegment(result, "IDLE", currentTime, nextTime);
            currentTime = nextTime;
            continue;
        }

        int index = readyQueue.pop().processIndex;
        int startTime = currentTime;
        currentTime += processes[index].burstTime;
        addSegment(result, processes[index].pid, startTime, currentTime);
        result.metrics[index].completionTime = currentTime;
        ++completed;
    }

    calculateFinalStatistics(result, processes, processCount, currentTime);
    return result;
}

ScheduleResult runRoundRobin(const Process processes[],
                             int processCount,
                             int timeQuantum) {
    if (timeQuantum <= 0) {
        throw invalid_argument("Time quantum must be greater than zero.");
    }

    ScheduleResult result;
    result.algorithm = "Round Robin (q=" + to_string(timeQuantum) + ")";

    int arrivalOrder[MAX_PROCESSES]{};
    int remainingTime[MAX_PROCESSES]{};
    buildArrivalOrder(processes, processCount, arrivalOrder);
    for (int i = 0; i < processCount; ++i) {
        remainingTime[i] = processes[i].burstTime;
    }

    CircularQueue readyQueue;
    int nextArrival = 0;
    int completed = 0;
    int currentTime = 0;

    while (completed < processCount) {
        while (nextArrival < processCount &&
               processes[arrivalOrder[nextArrival]].arrivalTime <= currentTime) {
            readyQueue.enqueue(arrivalOrder[nextArrival++]);
        }

        if (readyQueue.empty()) {
            int nextTime = processes[arrivalOrder[nextArrival]].arrivalTime;
            addSegment(result, "IDLE", currentTime, nextTime);
            currentTime = nextTime;
            continue;
        }

        int index = readyQueue.dequeue();
        int runTime = min(timeQuantum, remainingTime[index]);
        int startTime = currentTime;
        currentTime += runTime;
        remainingTime[index] -= runTime;
        addSegment(result, processes[index].pid, startTime, currentTime);

        // Processes arriving during the time slice enter the queue first.
        while (nextArrival < processCount &&
               processes[arrivalOrder[nextArrival]].arrivalTime <= currentTime) {
            readyQueue.enqueue(arrivalOrder[nextArrival++]);
        }

        if (remainingTime[index] > 0) {
            readyQueue.enqueue(index);
        } else {
            result.metrics[index].completionTime = currentTime;
            ++completed;
        }
    }

    calculateFinalStatistics(result, processes, processCount, currentTime);
    return result;
}

void printProcesses(const Process processes[], int processCount, int timeQuantum) {
    cout << "CPU SCHEDULING SIMULATOR USING DATA STRUCTURES\n";
    cout << "================================================\n";
    cout << "The program runs automatically using the built-in sample.\n";
    cout << "Priority rule: a smaller number means a higher priority.\n";
    cout << "Round Robin time quantum: " << timeQuantum << "\n";
    cout << "Context-switch cost: 0 time units.\n\n";

    cout << left << setw(10) << "Process"
         << setw(10) << "Arrival"
         << setw(10) << "Burst"
         << setw(10) << "Priority" << '\n';
    cout << string(40, '-') << '\n';
    for (int i = 0; i < processCount; ++i) {
        cout << left << setw(10) << processes[i].pid
             << setw(10) << processes[i].arrivalTime
             << setw(10) << processes[i].burstTime
             << setw(10) << processes[i].priority << '\n';
    }

    cout << "\nData structures: Array, Circular Queue, Binary Min-Heap, "
            "Linked List, and Stack.\n";
}

void printExecutionOrder(const ScheduleResult& result) {
    cout << "Execution order: ";
    bool first = true;
    for (int i = 0; i < result.segmentCount; ++i) {
        if (result.gantt[i].label == "IDLE") {
            continue;
        }
        if (!first) {
            cout << " -> ";
        }
        cout << result.gantt[i].label;
        first = false;
    }
    cout << '\n';
}

void printGanttChart(const ScheduleResult& result) {
    cout << "Gantt chart:     ";
    for (int i = 0; i < result.segmentCount; ++i) {
        const GanttSegment& segment = result.gantt[i];
        cout << '[' << segment.startTime << '-' << segment.endTime
             << ": " << segment.label << "] ";
    }
    cout << '\n';
}

void printReverseHistoryUsingStack(const ScheduleResult& result) {
    SegmentStack history;
    for (int i = 0; i < result.segmentCount; ++i) {
        if (result.gantt[i].label != "IDLE") {
            history.push(result.gantt[i]);
        }
    }

    cout << "Stack history (latest first): ";
    bool first = true;
    while (!history.empty()) {
        if (!first) {
            cout << " -> ";
        }
        cout << history.pop().label;
        first = false;
    }
    cout << '\n';
}

void printResult(const ScheduleResult& result,
                 const Process processes[],
                 int processCount) {
    cout << "\n\n" << string(76, '=') << '\n';
    cout << result.algorithm << '\n';
    cout << string(76, '=') << '\n';

    printExecutionOrder(result);
    printGanttChart(result);
    printReverseHistoryUsingStack(result);

    cout << "\n" << left
         << setw(9) << "PID"
         << setw(7) << "AT"
         << setw(7) << "BT"
         << setw(10) << "Priority"
         << setw(7) << "CT"
         << setw(7) << "WT"
         << setw(7) << "TAT" << '\n';
    cout << string(54, '-') << '\n';

    for (int i = 0; i < processCount; ++i) {
        cout << left
             << setw(9) << processes[i].pid
             << setw(7) << processes[i].arrivalTime
             << setw(7) << processes[i].burstTime
             << setw(10) << processes[i].priority
             << setw(7) << result.metrics[i].completionTime
             << setw(7) << result.metrics[i].waitingTime
             << setw(7) << result.metrics[i].turnaroundTime << '\n';
    }

    cout << fixed << setprecision(2);
    cout << "\nAverage Waiting Time    : " << result.averageWaitingTime << '\n';
    cout << "Average Turnaround Time : " << result.averageTurnaroundTime << '\n';
    cout << "CPU Busy Time           : " << result.busyTime << '\n';
    cout << "Total Simulation Time   : " << result.totalTime << '\n';
    cout << "CPU Idle Time           : " << result.totalTime - result.busyTime << '\n';
    cout << "CPU Utilization         : " << result.cpuUtilization << "%\n";
}

void printComparison(const ScheduleResult results[], int resultCount) {
    cout << "\n\n" << string(90, '=') << '\n';
    cout << "PERFORMANCE COMPARISON\n";
    cout << string(90, '=') << '\n';
    cout << left
         << setw(30) << "Algorithm"
         << setw(12) << "Avg WT"
         << setw(12) << "Avg TAT"
         << setw(12) << "Busy"
         << setw(12) << "Total"
         << setw(12) << "CPU Util." << '\n';
    cout << string(90, '-') << '\n';

    for (int i = 0; i < resultCount; ++i) {
        cout << left
             << setw(30) << results[i].algorithm
             << setw(12) << results[i].averageWaitingTime
             << setw(12) << results[i].averageTurnaroundTime
             << setw(12) << results[i].busyTime
             << setw(12) << results[i].totalTime
             << results[i].cpuUtilization << "%\n";
    }

    int bestWaitingIndex = 0;
    int bestTurnaroundIndex = 0;
    for (int i = 1; i < resultCount; ++i) {
        if (results[i].averageWaitingTime <
            results[bestWaitingIndex].averageWaitingTime) {
            bestWaitingIndex = i;
        }
        if (results[i].averageTurnaroundTime <
            results[bestTurnaroundIndex].averageTurnaroundTime) {
            bestTurnaroundIndex = i;
        }
    }

    cout << "\nLowest average waiting time    : "
         << results[bestWaitingIndex].algorithm << '\n';
    cout << "Lowest average turnaround time : "
         << results[bestTurnaroundIndex].algorithm << '\n';
    cout << "Note: utilization is equal here because context-switch cost is zero "
            "and all algorithms have the same initial idle interval.\n";
}

int main() {
    // Built-in sample: no keyboard input is required.
    Process sample[] = {
        {"P1", 2, 5, 3},
        {"P2", 3, 2, 1},
        {"P3", 4, 8, 4},
        {"P4", 5, 3, 2},
        {"P5", 7, 4, 1}
    };
    const int sampleSize = static_cast<int>(sizeof(sample) / sizeof(sample[0]));
    const int timeQuantum = 2;

    ProcessLinkedList processList;
    for (int i = 0; i < sampleSize; ++i) {
        processList.append(sample[i]);
    }

    Process processes[MAX_PROCESSES];
    int processCount = processList.copyToArray(processes, MAX_PROCESSES);

    printProcesses(processes, processCount, timeQuantum);

    ScheduleResult results[4];
    results[0] = runFCFS(processes, processCount);
    results[1] = runSJF(processes, processCount);
    results[2] = runPriority(processes, processCount);
    results[3] = runRoundRobin(processes, processCount, timeQuantum);

    for (const ScheduleResult& result : results) {
        printResult(result, processes, processCount);
    }
    printComparison(results, 4);

    return 0;
}
