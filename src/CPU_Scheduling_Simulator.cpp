#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

const int MAX_PROCESSES = 20;
const int TIME_QUANTUM = 2;
enum Algorithm { FCFS, SJF, PRIORITY, ROUND_ROBIN };
const char* algorithmNames[4] = {"FCFS", "SJF", "Priority", "Round Robin"};
struct Process { string id; int arrival, burst, priority; };
struct Slice { int process, start, end; }; // -1 means the CPU is idle.
struct Result {
    vector<int> completion;
    vector<Slice> timeline;
    double avgWaiting = 0, avgTurnaround = 0, utilization = 0;
};

// Linked List stores the original process records.
struct LinkedList {
    struct Node { Process value; Node* next; };
    Node* head = nullptr;
    void pushFront(const Process& p) { head = new Node{p, head}; }
    vector<Process> values() const {
        vector<Process> result;
        for (Node* node = head; node; node = node->next)
            result.push_back(node->value);
        return result;
    }
    ~LinkedList() {
        while (head) {
            Node* old = head;
            head = head->next;
            delete old;
        }
    }
};

// Circular Queue: first in, first out, used by FCFS and Round Robin.
struct Queue {
    int data[MAX_PROCESSES], head = 0, count = 0;
    bool empty() const { return count == 0; }
    void push(int value) {
        data[(head + count) % MAX_PROCESSES] = value;
        ++count;
    }
    int pop() {
        int value = data[head];
        head = (head + 1) % MAX_PROCESSES;
        --count;
        return value;
    }
};

// Sorted-array Priority Queue: the best process is at the end.
struct PriorityQueue {
    int data[MAX_PROCESSES], count = 0;
    const vector<Process>& processes;
    bool shortest;
    PriorityQueue(const vector<Process>& p, bool sjf) : processes(p), shortest(sjf) {}
    bool empty() const { return count == 0; }
    bool before(int a, int b) const {
        int ka = shortest ? processes[a].burst : processes[a].priority;
        int kb = shortest ? processes[b].burst : processes[b].priority;
        if (ka != kb) return ka < kb;
        if (processes[a].arrival != processes[b].arrival)
            return processes[a].arrival < processes[b].arrival;
        return a < b;
    }
    void push(int value) {
        int position = count++;
        while (position > 0 && before(data[position - 1], value)) {
            data[position] = data[position - 1];
            --position;
        }
        data[position] = value;
    }
    int pop() { return data[--count]; }
};

// Linked Stack: last in, first out, used for reverse execution history.
struct Stack {
    struct Node { Slice value; Node* next; };
    Node* top = nullptr;
    bool empty() const { return top == nullptr; }
    void push(const Slice& value) { top = new Node{value, top}; }
    Slice pop() {
        Node* old = top;
        Slice value = old->value;
        top = old->next;
        delete old;
        return value;
    }
    ~Stack() { while (!empty()) pop(); }
};

vector<Process> sample(bool comparison) {
    const Process assignment[] = {{"P1",0,5,2}, {"P2",1,3,1}, {"P3",2,8,3}};
    const Process extended[] = {
        {"P1",2,5,3}, {"P2",3,2,1}, {"P3",4,8,4}, {"P4",5,3,2}, {"P5",7,4,1}
    };
    const Process* input = comparison ? extended : assignment;
    LinkedList list;
    // Insert backwards so the list retains the sample's original order.
    for (int i = (comparison ? 5 : 3) - 1; i >= 0; --i)
        list.pushFront(input[i]);
    return list.values();
}

// One simulation loop; the selection rule distinguishes the algorithms.
Result schedule(const vector<Process>& p, int algorithm, int quantum = TIME_QUANTUM) {
    int n = static_cast<int>(p.size());
    if (n < 1 || n > MAX_PROCESSES || algorithm < FCFS || algorithm > ROUND_ROBIN || quantum < 1)
        throw invalid_argument("Invalid process count, algorithm or time quantum.");
    int order[MAX_PROCESSES], remaining[MAX_PROCESSES], lastArrival = 0;
    long long busy = 0;
    for (int i = 0; i < n; ++i) {
        if (p[i].arrival < 0 || p[i].burst <= 0)
            throw invalid_argument("Arrival must be nonnegative and burst positive.");
        busy += p[i].burst;
        lastArrival = max(lastArrival, p[i].arrival);
        remaining[i] = p[i].burst;
        order[i] = i;
        // Stable insertion sort: arrival time, then original input order.
        for (int j = i; j > 0 && p[order[j]].arrival < p[order[j - 1]].arrival; --j)
            swap(order[j], order[j - 1]);
    }
    if (busy + lastArrival > numeric_limits<int>::max())
        throw invalid_argument("Simulation time exceeds the supported range.");
    Result result;
    result.completion.resize(n);
    Queue queue;
    PriorityQueue priorityQueue(p, algorithm == SJF);
    bool usePriority = algorithm == SJF || algorithm == PRIORITY;
    int time = 0, next = 0, finished = 0, paused = -1;
    while (finished < n) {
        while (next < n && p[order[next]].arrival <= time) {
            int index = order[next++];
            if (usePriority) priorityQueue.push(index);
            else queue.push(index);
        }
        // In RR, new arrivals enter before the previous process returns.
        if (paused >= 0) {
            queue.push(paused);
            paused = -1;
        }
        if (usePriority ? priorityQueue.empty() : queue.empty()) {
            int nextTime = p[order[next]].arrival;
            result.timeline.push_back({-1, time, nextTime});
            time = nextTime;
            continue;
        }
        int index = usePriority ? priorityQueue.pop() : queue.pop();
        int duration = algorithm == ROUND_ROBIN ? min(quantum, remaining[index]) : remaining[index];
        result.timeline.push_back({index, time, time + duration});
        time += duration;
        remaining[index] -= duration;
        if (remaining[index] == 0) {
            result.completion[index] = time;
            ++finished;
        } else paused = index;
    }
    for (int i = 0; i < n; ++i) {
        int turnaround = result.completion[i] - p[i].arrival;
        result.avgTurnaround += turnaround;
        result.avgWaiting += turnaround - p[i].burst;
    }
    result.avgWaiting /= n;
    result.avgTurnaround /= n;
    result.utilization = 100.0 * static_cast<double>(busy) / time;
    return result;
}

string formatResults(const vector<Process>& p, const vector<Result>& results,
                     int selected, int quantum) {
    const Result& r = results.at(selected);
    ostringstream out;
    out << fixed << setprecision(2);
    out << algorithmNames[selected] << "  |  RR quantum = " << quantum << "\r\n\r\n";
    out << "PID    AT    BT   PRI    CT    WT   TAT\r\n";
    for (size_t i = 0; i < p.size(); ++i) {
        int turnaround = r.completion[i] - p[i].arrival;
        out << left << setw(5) << p[i].id << right << setw(4) << p[i].arrival
            << setw(6) << p[i].burst << setw(6) << p[i].priority
            << setw(6) << r.completion[i] << setw(6) << turnaround - p[i].burst
            << setw(6) << turnaround << "\r\n";
    }
    out << "\r\nAverage WT: " << r.avgWaiting << "   Average TAT: " << r.avgTurnaround
        << "   CPU utilization: " << r.utilization << "%\r\n";
    out << "\r\nExecution order / Gantt intervals:\r\n";
    Stack history;
    int shown = 0;
    for (const Slice& s : r.timeline) {
        out << (s.process < 0 ? "IDLE" : p[s.process].id)
            << "[" << s.start << "," << s.end << ")  ";
        history.push(s);
        if (++shown % 6 == 0) out << "\r\n";
    }
    out << "\r\n\r\nReverse history (Stack):\r\n";
    shown = 0;
    while (!history.empty()) {
        Slice s = history.pop();
        out << (s.process < 0 ? "IDLE" : p[s.process].id)
            << "[" << s.start << "," << s.end << ")  ";
        if (++shown % 6 == 0) out << "\r\n";
    }
    out << "\r\n\r\nComparison             Avg WT    Avg TAT    CPU %\r\n";
    for (int a = 0; a < 4; ++a)
        out << left << setw(20) << algorithmNames[a] << right
            << setw(9) << results.at(a).avgWaiting << setw(11) << results.at(a).avgTurnaround
            << setw(9) << results.at(a).utilization << "\r\n";
    out << "\r\nAT=Arrival  BT=Burst  PRI=Priority  CT=Completion\r\n"
        << "WT=Waiting  TAT=Turnaround  Lower priority number runs first.\r\n";
    return out.str();
}

#ifdef _WIN32
int selectedAlgorithm = FCFS;
bool comparisonSample = false;
HFONT resultFont = nullptr;

// Each button displays results from the same scheduling function.
void showResults(HWND window) {
    bool comparison = IsDlgButtonChecked(window, 105) == BST_CHECKED;
    vector<Process> processes = sample(comparison);
    vector<Result> results;
    for (int algorithm = 0; algorithm < 4; ++algorithm)
        results.push_back(schedule(processes, algorithm, TIME_QUANTUM));
    string text = formatResults(processes, results, selectedAlgorithm, TIME_QUANTUM);
    SetDlgItemTextA(window, 106, text.c_str());
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        for (int i = 0; i < 4; ++i) {
            HWND button = CreateWindowA("BUTTON", algorithmNames[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                12 + i * 130, 10, 120, 30, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(101 + i)), nullptr, nullptr);
            SendMessageA(button, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        HWND check = CreateWindowA("BUTTON", "Comparison sample", WS_CHILD | WS_VISIBLE |
            WS_TABSTOP | BS_AUTOCHECKBOX, 550, 10, 200, 30, window,
            reinterpret_cast<HMENU>(105), nullptr, nullptr);
        SendMessageA(check, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        CheckDlgButton(window, 105, comparisonSample ? BST_CHECKED : BST_UNCHECKED);
        HWND output = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE |
            WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL,
            12, 52, 950, 550, window, reinterpret_cast<HMENU>(106), nullptr, nullptr);
        resultFont = CreateFontA(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
        SendMessageA(output, WM_SETFONT, reinterpret_cast<WPARAM>(resultFont), TRUE);
        showResults(window);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) >= 101 && LOWORD(wParam) <= 104)
            selectedAlgorithm = LOWORD(wParam) - 101;
        if (LOWORD(wParam) >= 101 && LOWORD(wParam) <= 105)
            showResults(window);
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            MoveWindow(GetDlgItem(window, 106), 12, 52, LOWORD(lParam) - 24, HIWORD(lParam) - 64, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {800, 500};
        return 0;
    case WM_DESTROY:
        DeleteObject(resultFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

int runWindow(bool comparison) {
    FreeConsole();
    comparisonSample = comparison;
    WNDCLASSA type{};
    type.lpfnWndProc = windowProcedure;
    type.hInstance = GetModuleHandleA(nullptr);
    type.hCursor = LoadCursor(nullptr, IDC_ARROW);
    type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    type.lpszClassName = "SchedulingWindow";
    if (!RegisterClassA(&type)) throw runtime_error("Cannot register the window.");
    HWND window = CreateWindowA(type.lpszClassName, "CPU Scheduling Algorithms Simulation Using Data Structures",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1050, 700,
        nullptr, nullptr, type.hInstance, nullptr);
    if (!window) throw runtime_error("Cannot open the window.");
    ShowWindow(window, SW_SHOW);
    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    return 0;
}
#endif

int main(int argc, char* argv[]) {
    bool comparison = false;
    bool console = false;
    for (int i = 1; i < argc; ++i) {
        string option = argv[i];
        if (option == "--compare") comparison = true;
        else if (option == "--console") console = true;
        else {
            cout << "Usage: CPU_Scheduling_Simulator [--console] [--compare]\n";
            return option == "--help" ? 0 : 1;
        }
    }
    try {
#ifdef _WIN32
        if (!console) return runWindow(comparison);
#else
        (void)console;
#endif
        vector<Process> processes = sample(comparison);
        vector<Result> results;
        for (int algorithm = 0; algorithm < 4; ++algorithm)
            results.push_back(schedule(processes, algorithm, TIME_QUANTUM));
        for (int algorithm = 0; algorithm < 4; ++algorithm)
            cout << formatResults(processes, results, algorithm, TIME_QUANTUM) << '\n';
    } catch (const exception& error) {
#ifdef _WIN32
        if (!console) MessageBoxA(nullptr, error.what(), "Error", MB_OK | MB_ICONERROR);
        else
#endif
            cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
