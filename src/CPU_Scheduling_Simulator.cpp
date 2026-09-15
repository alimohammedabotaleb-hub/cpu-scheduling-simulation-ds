#include <algorithm>
#include <climits>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

    // Sort process indices by arrival, preserving the input order for ties.
    stable_sort(order, order + processCount, [&](int left, int right) {
        if (processes[left].arrivalTime != processes[right].arrivalTime) {
            return processes[left].arrivalTime < processes[right].arrivalTime;
        }
        return left < right;
    });
}

void validateProcesses(const Process processes[], int processCount) {
    if (processCount < 1 || processCount > MAX_PROCESSES) {
        throw invalid_argument("Process count must be between 1 and " +
                               to_string(MAX_PROCESSES) + ".");
    }

    long long totalBurst = 0;
    long long latestArrival = 0;
    for (int i = 0; i < processCount; ++i) {
        if (processes[i].pid.empty()) {
            throw invalid_argument("Process IDs must not be empty.");
        }
        for (int j = 0; j < i; ++j) {
            if (processes[i].pid == processes[j].pid) {
                throw invalid_argument("Process IDs must be unique.");
            }
        }
        if (processes[i].arrivalTime < 0 || processes[i].burstTime <= 0) {
            throw invalid_argument("Arrival time must be nonnegative and burst time must be positive.");
        }
        totalBurst += processes[i].burstTime;
        latestArrival = max(latestArrival,
                            static_cast<long long>(processes[i].arrivalTime));
    }
    if (latestArrival + totalBurst > INT_MAX) {
        throw invalid_argument("Process times exceed the supported integer range.");
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
    validateProcesses(processes, processCount);
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
    validateProcesses(processes, processCount);
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
    validateProcesses(processes, processCount);
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
    validateProcesses(processes, processCount);
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
    cout << "CPU Scheduling Algorithms Simulation Using Data Structures\n";
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

    cout << "\nLowest average waiting time    : ";
    bool first = true;
    for (int i = 0; i < resultCount; ++i) {
        if (results[i].averageWaitingTime == results[bestWaitingIndex].averageWaitingTime) {
            if (!first) {
                cout << ", ";
            }
            cout << results[i].algorithm;
            first = false;
        }
    }
    cout << "\nLowest average turnaround time : ";
    first = true;
    for (int i = 0; i < resultCount; ++i) {
        if (results[i].averageTurnaroundTime == results[bestTurnaroundIndex].averageTurnaroundTime) {
            if (!first) {
                cout << ", ";
            }
            cout << results[i].algorithm;
            first = false;
        }
    }
    cout << "\nNote: utilization is equal for these work-conserving schedules "
            "with zero context-switch cost and the same process workload.\n";
}

// Shared sample loading and execution for the console and desktop interface.
int loadSample(bool comparison, Process destination[], int capacity) {
    const Process assignmentSample[] = {
        {"P1", 0, 5, 2}, {"P2", 1, 3, 1}, {"P3", 2, 8, 3}
    };
    const Process comparisonSample[] = {
        {"P1", 2, 5, 3}, {"P2", 3, 2, 1}, {"P3", 4, 8, 4},
        {"P4", 5, 3, 2}, {"P5", 7, 4, 1}
    };
    const Process* sample = comparison ? comparisonSample : assignmentSample;
    const int count = comparison ? 5 : 3;
    ProcessLinkedList processList;
    for (int i = 0; i < count; ++i) {
        processList.append(sample[i]);
    }
    return processList.copyToArray(destination, capacity);
}

void runAllAlgorithms(const Process processes[], int count, int quantum,
                      ScheduleResult results[4]) {
    results[0] = runFCFS(processes, count);
    results[1] = runSJF(processes, count);
    results[2] = runPriority(processes, count);
    results[3] = runRoundRobin(processes, count, quantum);
}

#ifdef _WIN32
// Native Windows presentation: all displayed values come from the core above.
enum GuiControlId {
    ID_SAMPLE = 101, ID_ALGORITHM, ID_QUANTUM, ID_RUN_ALL,
    ID_NEXT_STEP, ID_RESET_REPLAY
};

struct GuiState {
    Process processes[MAX_PROCESSES];
    ScheduleResult results[4];
    int processCount = 0;
    int algorithmIndex = 0;
    int quantum = 2;
    int replayStep = 0;
    bool comparison = false;
    HWND sampleControl = nullptr;
    HWND algorithmControl = nullptr;
    HWND quantumControl = nullptr;
    HWND nextControl = nullptr;
    HFONT bodyFont = nullptr;
    HFONT smallFont = nullptr;
    HFONT headingFont = nullptr;
    HFONT titleFont = nullptr;

    ~GuiState() {
        DeleteObject(bodyFont);
        DeleteObject(smallFont);
        DeleteObject(headingFont);
        DeleteObject(titleFont);
    }
};

const COLORREF GUI_NAVY = RGB(24, 44, 73);
const COLORREF GUI_TEXT = RGB(29, 43, 61);
const COLORREF GUI_MUTED = RGB(83, 99, 119);
const COLORREF GUI_BACKGROUND = RGB(242, 245, 249);

wstring wideText(const string& text) {
    // Process identifiers and console labels are ASCII in both built-in samples.
    return wstring(text.begin(), text.end());
}

wstring decimalText(double value) {
    wostringstream text;
    text << fixed << setprecision(2) << value;
    return text.str();
}

HFONT createGuiFont(int height, int weight = FW_NORMAL) {
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

void fillArea(HDC dc, const RECT& area, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &area, brush);
    DeleteObject(brush);
}

void drawGuiText(HDC dc, const wstring& text, RECT area, HFONT font,
                 COLORREF color = GUI_TEXT,
                 UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &area,
              format | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

void drawTableCell(HDC dc, const wstring& text, int x, int y, int width,
                   int height, HFONT font, bool leftAligned = false,
                   COLORREF color = GUI_TEXT) {
    drawGuiText(dc, text, {x + 5, y, x + width - 5, y + height}, font, color,
                DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                (leftAligned ? DT_LEFT : DT_CENTER));
}

COLORREF processColor(const string& pid) {
    const COLORREF colors[] = {
        RGB(33, 102, 172), RGB(21, 124, 107), RGB(123, 78, 163),
        RGB(167, 96, 28), RGB(174, 64, 93)
    };
    if (pid == "IDLE") {
        return RGB(126, 136, 149);
    }
    if (pid.size() == 2 && pid[1] >= '1' && pid[1] <= '5') {
        return colors[pid[1] - '1'];
    }
    return GUI_NAVY;
}

wstring executionText(const ScheduleResult& result, bool reverse) {
    wstring text;
    if (reverse) {
        // The same explicit stack used in the console also supplies this view.
        auto history = make_unique<SegmentStack>();
        for (int i = 0; i < result.segmentCount; ++i) {
            if (result.gantt[i].label != "IDLE") {
                history->push(result.gantt[i]);
            }
        }
        while (!history->empty()) {
            if (!text.empty()) text += L" \u2192 ";
            text += wideText(history->pop().label);
        }
    } else {
        for (int i = 0; i < result.segmentCount; ++i) {
            if (result.gantt[i].label == "IDLE") continue;
            if (!text.empty()) text += L" \u2192 ";
            text += wideText(result.gantt[i].label);
        }
    }
    return text;
}

void refreshGuiResults(HWND window, GuiState& state) {
    state.comparison = SendMessageW(state.sampleControl, CB_GETCURSEL, 0, 0) == 1;
    state.quantum = static_cast<int>(SendMessageW(state.quantumControl,
                                                  CB_GETCURSEL, 0, 0)) + 1;
    state.processCount = loadSample(state.comparison, state.processes, MAX_PROCESSES);
    runAllAlgorithms(state.processes, state.processCount, state.quantum, state.results);
    state.replayStep = 0;
    EnableWindow(state.nextControl, TRUE);
    InvalidateRect(window, nullptr, FALSE);
}

HWND addGuiControl(HWND window, const wchar_t* className, const wchar_t* label,
                    DWORD style, int id, int x, int width, HFONT font) {
    HWND control = CreateWindowExW(0, className, label,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, x, 120, width,
        (style & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST ? 220 : 32,
        window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (!control) throw runtime_error("Could not create a desktop control.");
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
}

void createGuiControls(HWND window, GuiState& state) {
    state.bodyFont = createGuiFont(16);
    state.smallFont = createGuiFont(13);
    state.headingFont = createGuiFont(18, FW_SEMIBOLD);
    state.titleFont = createGuiFont(26, FW_SEMIBOLD);
    const DWORD comboStyle = CBS_DROPDOWNLIST | WS_VSCROLL;
    state.sampleControl = addGuiControl(window, L"COMBOBOX", L"", comboStyle,
                                         ID_SAMPLE, 24, 226, state.bodyFont);
    SendMessageW(state.sampleControl, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Assignment (3 processes)"));
    SendMessageW(state.sampleControl, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Comparison (5 processes)"));
    SendMessageW(state.sampleControl, CB_SETCURSEL, state.comparison ? 1 : 0, 0);

    state.algorithmControl = addGuiControl(window, L"COMBOBOX", L"", comboStyle,
                                            ID_ALGORITHM, 264, 162, state.bodyFont);
    for (const wchar_t* name : {L"FCFS", L"SJF", L"Priority", L"Round Robin"}) {
        SendMessageW(state.algorithmControl, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(name));
    }
    SendMessageW(state.algorithmControl, CB_SETCURSEL, 0, 0);
    state.quantumControl = addGuiControl(window, L"COMBOBOX", L"", comboStyle,
                                          ID_QUANTUM, 440, 60, state.bodyFont);
    for (int quantum = 1; quantum <= 5; ++quantum) {
        wstring value = to_wstring(quantum);
        SendMessageW(state.quantumControl, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(value.c_str()));
    }
    SendMessageW(state.quantumControl, CB_SETCURSEL, 1, 0);
    addGuiControl(window, L"BUTTON", L"Run all", BS_PUSHBUTTON,
                   ID_RUN_ALL, 514, 116, state.bodyFont);
    state.nextControl = addGuiControl(window, L"BUTTON", L"Next step", BS_PUSHBUTTON,
                                       ID_NEXT_STEP, 644, 126, state.bodyFont);
    addGuiControl(window, L"BUTTON", L"Reset replay", BS_PUSHBUTTON,
                   ID_RESET_REPLAY, 784, 122, state.bodyFont);
    refreshGuiResults(window, state);
}

void paintProcessTable(HDC dc, const GuiState& state, int right) {
    const ScheduleResult& result = state.results[state.algorithmIndex];
    fillArea(dc, {24, 205, right, 403}, RGB(255, 255, 255));
    const int tableLeft = 34;
    const int width = right - tableLeft - 10;
    const int column = width / 7;
    fillArea(dc, {tableLeft, 215, right - 10, 244}, RGB(229, 235, 243));
    const wchar_t* headers[] = {L"PID", L"AT", L"BT", L"Priority", L"CT", L"WT", L"TAT"};
    for (int c = 0; c < 7; ++c) {
        drawTableCell(dc, headers[c], tableLeft + c * column, 215, column, 29,
                      state.bodyFont);
    }
    for (int i = 0; i < state.processCount; ++i) {
        const int y = 244 + i * 26;
        if (i % 2 == 1) fillArea(dc, {tableLeft, y, right - 10, y + 26}, RGB(246, 248, 251));
        const Process& process = state.processes[i];
        const ProcessMetrics& metric = result.metrics[i];
        const wstring values[] = {
            wideText(process.pid), to_wstring(process.arrivalTime),
            to_wstring(process.burstTime), to_wstring(process.priority),
            to_wstring(metric.completionTime), to_wstring(metric.waitingTime),
            to_wstring(metric.turnaroundTime)
        };
        for (int c = 0; c < 7; ++c) {
            drawTableCell(dc, values[c], tableLeft + c * column, y, column, 26,
                          state.bodyFont, false, c == 0 ? processColor(process.pid) : GUI_TEXT);
        }
    }
    drawGuiText(dc, L"AT arrival  |  BT burst  |  CT completion  |  WT waiting  |  TAT turnaround",
                {34, 379, right - 10, 397}, state.smallFont, GUI_MUTED);
}

void paintComparisonTable(HDC dc, const GuiState& state, int left, int right) {
    fillArea(dc, {left, 205, right, 403}, RGB(255, 255, 255));
    const int x = left + 10;
    const int width = right - left - 20;
    const int firstWidth = width * 36 / 100;
    const int numberWidth = (width - firstWidth) / 3;
    fillArea(dc, {x, 215, right - 10, 244}, RGB(229, 235, 243));
    drawTableCell(dc, L"Algorithm", x, 215, firstWidth, 29, state.bodyFont, true);
    const wchar_t* headers[] = {L"Avg WT", L"Avg TAT", L"CPU %"};
    for (int c = 0; c < 3; ++c) {
        drawTableCell(dc, headers[c], x + firstWidth + c * numberWidth, 215,
                      numberWidth, 29, state.smallFont);
    }
    const wstring names[] = {L"FCFS", L"SJF", L"Priority", L"RR (q=" + to_wstring(state.quantum) + L")"};
    for (int i = 0; i < 4; ++i) {
        int y = 244 + i * 28;
        if (i == state.algorithmIndex) {
            fillArea(dc, {x, y, right - 10, y + 28}, RGB(222, 235, 249));
        } else if (i % 2 == 1) {
            fillArea(dc, {x, y, right - 10, y + 28}, RGB(246, 248, 251));
        }
        const ScheduleResult& result = state.results[i];
        drawTableCell(dc, names[i], x, y, firstWidth, 28, state.bodyFont, true);
        const wstring values[] = {decimalText(result.averageWaitingTime),
                                  decimalText(result.averageTurnaroundTime),
                                  decimalText(result.cpuUtilization)};
        for (int c = 0; c < 3; ++c) {
            drawTableCell(dc, values[c], x + firstWidth + c * numberWidth, y,
                          numberWidth, 28, state.bodyFont);
        }
    }
    drawGuiText(dc, state.comparison
        ? L"Compare waiting times; utilization is equal for this workload."
        : L"FCFS, SJF and Priority tie on the assignment sample.",
        {x + 4, 363, right - 12, 397}, state.smallFont, GUI_MUTED, DT_LEFT | DT_WORDBREAK);
}

void paintGanttChart(HDC dc, const GuiState& state, int width) {
    const ScheduleResult& result = state.results[state.algorithmIndex];
    const int chartLeft = 34;
    const int chartWidth = width - 68;
    fillArea(dc, {24, 472, width - 24, 543}, RGB(255, 255, 255));
    for (int i = 0; i < result.segmentCount; ++i) {
        const GanttSegment& segment = result.gantt[i];
        const int left = chartLeft + chartWidth * segment.startTime / result.totalTime;
        const int right = chartLeft + chartWidth * segment.endTime / result.totalTime;
        fillArea(dc, {left, 482, right - 1, 513}, processColor(segment.label));
        drawGuiText(dc, wideText(segment.label), {left, 482, right - 1, 513},
                    state.smallFont, RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (state.replayStep == i + 1) {
            HPEN pen = CreatePen(PS_SOLID, 3, RGB(232, 174, 41));
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, left - 1, 480, right, 515);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }
        drawGuiText(dc, to_wstring(segment.startTime), {left - 14, 516, left + 18, 538},
                    state.smallFont, GUI_MUTED, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    drawGuiText(dc, to_wstring(result.totalTime),
                {chartLeft + chartWidth - 18, 516, chartLeft + chartWidth + 14, 538},
                state.smallFont, GUI_MUTED, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paintGui(HDC dc, const RECT& area, const GuiState& state) {
    const int width = area.right;
    const int tableRight = 24 + (width - 66) * 54 / 100;
    const int comparisonLeft = tableRight + 18;
    const ScheduleResult& result = state.results[state.algorithmIndex];
    fillArea(dc, area, GUI_BACKGROUND);
    fillArea(dc, {0, 0, width, 84}, GUI_NAVY);
    drawGuiText(dc, L"CPU Scheduling Simulator", {24, 13, width - 24, 47},
                state.titleFont, RGB(255, 255, 255));
    drawGuiText(dc, L"C++  |  FCFS, SJF, Priority and Round Robin  |  Queue, Priority Queue, Linked List and Stack",
                {25, 49, width - 24, 73}, state.smallFont, RGB(206, 222, 240));
    drawGuiText(dc, L"Sample", {24, 96, 250, 116}, state.smallFont, GUI_MUTED);
    drawGuiText(dc, L"Algorithm", {264, 96, 426, 116}, state.smallFont, GUI_MUTED);
    drawGuiText(dc, L"RR quantum", {440, 96, 513, 116}, state.smallFont, GUI_MUTED);
    drawGuiText(dc, L"Process results - " + wideText(result.algorithm),
                {24, 171, tableRight, 198}, state.headingFont);
    drawGuiText(dc, L"Performance comparison", {comparisonLeft, 171, width - 24, 198},
                state.headingFont);
    paintProcessTable(dc, state, tableRight);
    paintComparisonTable(dc, state, comparisonLeft, width - 24);
    const wchar_t* explanations[] = {
        L"FCFS: ready processes execute in arrival order using a circular FIFO queue.",
        L"SJF: a min-heap selects the shortest ready burst; the selected process finishes without preemption.",
        L"Priority: a min-heap selects the smallest priority number; ties use arrival time, then input order.",
        L"Round Robin: a circular queue gives each process a time slice; unfinished processes return to the rear."
    };
    drawGuiText(dc, explanations[state.algorithmIndex], {24, 413, width - 24, 439},
                state.smallFont, GUI_MUTED);
    drawGuiText(dc, L"Gantt chart - proportional logical time", {24, 445, width - 24, 470},
                state.headingFont);
    paintGanttChart(dc, state, width);
    wstring replay;
    if (state.replayStep == 0) {
        replay = L"Replay ready. Next step highlights one execution slice; tables show the final results.";
    } else {
        const GanttSegment& segment = result.gantt[state.replayStep - 1];
        replay = L"Step " + to_wstring(state.replayStep) + L" / " + to_wstring(result.segmentCount)
            + L": " + wideText(segment.label) + L" runs from " + to_wstring(segment.startTime)
            + L" to " + to_wstring(segment.endTime) + L" (" + to_wstring(segment.endTime - segment.startTime)
            + L" time units).";
    }
    drawGuiText(dc, replay, {24, 551, width - 24, 576}, state.bodyFont);
    drawGuiText(dc, L"Execution order", {24, 584, 204, 608}, state.smallFont, GUI_MUTED);
    drawGuiText(dc, executionText(result, false), {204, 584, width - 24, 608}, state.smallFont);
    drawGuiText(dc, L"Stack: latest first", {24, 614, 204, 638}, state.smallFont, GUI_MUTED);
    drawGuiText(dc, executionText(result, true), {204, 614, width - 24, 638}, state.smallFont);
    drawGuiText(dc,
        L"WT = CT - AT - BT    |    TAT = CT - AT    |    Single CPU, no I/O, zero context-switch cost    |    Built-in samples",
        {24, area.bottom - 31, width - 24, area.bottom - 10}, state.smallFont, GUI_MUTED);
}

LRESULT CALLBACK guiWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    GuiState* state = reinterpret_cast<GuiState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    try {
        switch (message) {
        case WM_NCCREATE: {
            auto creation = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = static_cast<GuiState*>(creation->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            return TRUE;
        }
        case WM_CREATE:
            createGuiControls(window, *state);
            return 0;
        case WM_GETMINMAXINFO: {
            auto limits = reinterpret_cast<MINMAXINFO*>(lParam);
            RECT minimum = {0, 0, 960, 680};
            AdjustWindowRect(&minimum, WS_OVERLAPPEDWINDOW, FALSE);
            limits->ptMinTrackSize = {minimum.right - minimum.left, minimum.bottom - minimum.top};
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int action = HIWORD(wParam);
            if ((id == ID_SAMPLE || id == ID_QUANTUM) && action == CBN_SELCHANGE) {
                refreshGuiResults(window, *state);
            } else if (id == ID_ALGORITHM && action == CBN_SELCHANGE) {
                state->algorithmIndex = static_cast<int>(SendMessageW(state->algorithmControl,
                                                                        CB_GETCURSEL, 0, 0));
                state->replayStep = 0;
                EnableWindow(state->nextControl, TRUE);
                InvalidateRect(window, nullptr, FALSE);
            } else if (id == ID_RUN_ALL && action == BN_CLICKED) {
                refreshGuiResults(window, *state);
            } else if (id == ID_NEXT_STEP && action == BN_CLICKED) {
                const int count = state->results[state->algorithmIndex].segmentCount;
                if (state->replayStep < count) ++state->replayStep;
                EnableWindow(state->nextControl, state->replayStep < count);
                InvalidateRect(window, nullptr, FALSE);
            } else if (id == ID_RESET_REPLAY && action == BN_CLICKED) {
                state->replayStep = 0;
                EnableWindow(state->nextControl, TRUE);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_PRINTCLIENT: {
            RECT area;
            GetClientRect(window, &area);
            paintGui(reinterpret_cast<HDC>(wParam), area, *state);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            RECT area;
            GetClientRect(window, &area);
            // Paint into a back buffer so replay and selection changes do not flicker.
            HDC buffer = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, area.right, area.bottom);
            HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
            paintGui(buffer, area, *state);
            BitBlt(dc, 0, 0, area.right, area.bottom, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(buffer);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    } catch (const exception& error) {
        const wstring text = wideText(error.what());
        MessageBoxW(window, text.c_str(), L"CPU Scheduling Simulator", MB_OK | MB_ICONERROR);
        if (message == WM_CREATE) return -1;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int runDesktopGui(bool comparison) {
    // Keep the large result/segment arrays on the heap, away from the Windows stack.
    auto state = make_unique<GuiState>();
    state->comparison = comparison;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"CpuSchedulingSimulatorWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = guiWindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) {
        throw runtime_error("Could not register the desktop window.");
    }
    RECT size = {0, 0, 1100, 680};
    AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(0, className,
        L"CPU Scheduling Algorithms Simulation Using Data Structures",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        size.right - size.left, size.bottom - size.top, nullptr, nullptr, instance, state.get());
    if (!window) throw runtime_error("Could not open the desktop window.");
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    int received;
    while ((received = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    UnregisterClassW(className, instance);
    if (received == -1) throw runtime_error("The desktop message loop failed.");
    return static_cast<int>(message.wParam);
}
#endif

int main(int argc, char* argv[]) {
    const string usage =
        "Usage: cpu_scheduler [--console] [--compare] [--gui] [--help]\n"
        "  No arguments: Windows desktop GUI; console on other platforms.\n"
        "  --console: run the three-process assignment sample in the console.\n"
        "  --compare: run the five-process comparison sample in the console.\n"
        "  --gui: open the Windows GUI (may be combined with --compare).\n"
        "  --help: show these instructions.\n";
    bool comparisonSample = false;
    bool consoleRequested = false;
    bool guiRequested = false;
    for (int i = 1; i < argc; ++i) {
        const string argument = argv[i];
        if (argument == "--help") {
            cout << usage;
            return 0;
        }
        if (argument == "--compare") comparisonSample = true;
        else if (argument == "--console") consoleRequested = true;
        else if (argument == "--gui") guiRequested = true;
        else {
            cerr << usage;
            return 2;
        }
    }
    if (guiRequested && consoleRequested) {
        cerr << "Choose either --gui or --console.\n" << usage;
        return 2;
    }
    try {
#ifdef _WIN32
        if (guiRequested || (!consoleRequested && !comparisonSample)) {
            // Detach this program from the console; do not hide the user's terminal.
            FreeConsole();
            return runDesktopGui(comparisonSample);
        }
#else
        if (guiRequested) {
            cerr << "The desktop GUI requires Windows. Use --console on this platform.\n";
            return 2;
        }
#endif
        Process processes[MAX_PROCESSES];
        const int count = loadSample(comparisonSample, processes, MAX_PROCESSES);
        const int quantum = 2;
        auto results = make_unique<ScheduleResult[]>(4);
        runAllAlgorithms(processes, count, quantum, results.get());
        printProcesses(processes, count, quantum);
        for (int i = 0; i < 4; ++i) printResult(results[i], processes, count);
        printComparison(results.get(), 4);
        return 0;
    } catch (const exception& error) {
#ifdef _WIN32
        if (guiRequested || (!consoleRequested && !comparisonSample)) {
            const wstring text = wideText(error.what());
            MessageBoxW(nullptr, text.c_str(), L"CPU Scheduling Simulator", MB_OK | MB_ICONERROR);
        } else
#endif
        {
            cerr << "Error: " << error.what() << '\n';
        }
        return 1;
    }
}
