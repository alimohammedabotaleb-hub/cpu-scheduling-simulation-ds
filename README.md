# CPU Scheduling Simulator Using Appropriate Data Structures

مشروع لمقرر **هياكل البيانات والخوارزميات – نظري** يحاكي أشهر خوارزميات جدولة المعالج باستخدام لغة C++17 وهياكل بيانات مبنية داخل الكود.

**رابط المستودع:** https://github.com/alimohammedabotaleb-hub/cpu-scheduling-simulation-ds

## تشغيل المشروع داخل GitHub

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/alimohammedabotaleb-hub/cpu-scheduling-simulation-ds?quickstart=1)

[![Build and run](https://github.com/alimohammedabotaleb-hub/cpu-scheduling-simulation-ds/actions/workflows/run-project.yml/badge.svg)](https://github.com/alimohammedabotaleb-hub/cpu-scheduling-simulation-ds/actions/workflows/run-project.yml)

افتح Codespaces من الزر أعلاه، ثم أنشئ البيئة أو استأنف البيئة الموجودة. بعد فتح المحرر، شغّل المشروع من الطرفية بالأمر:

```bash
bash run_github.sh
```

يمكن أيضًا الضغط على `Ctrl+Shift+B` داخل المحرر. لا تحتاج إلى إدخال عمليات؛ تظهر نتائج الخوارزميات الأربع مباشرة، وتُحفظ في `output/Live_Output.txt`.

يتولى GitHub Actions تجميع الكود وتشغيله على Linux وWindows والتحقق من مطابقته لنتائج العينة المرجعية، ويتيح تنزيل الملف التنفيذي والنتائج. افتح شارة التشغيل أعلاه لعرض النتائج أو اختر `Run workflow` لإعادة التنفيذ.

الخطوات التفصيلية: [دليل التشغيل داخل GitHub](docs/GitHub_Run_Guide_AR.md). تتطلب Codespaces تسجيل الدخول وتوفر الحصة في الحساب.

## بيانات المشروع

- إعداد: مؤمن عبدالله الشامي
- إشراف: د. بلال مرشد
- لغة البرمجة: C++17
- نمط التشغيل: تلقائي باستخدام عينة ثابتة داخل الكود، من دون إدخال من لوحة المفاتيح

## الخوارزميات المطبقة

1. FCFS (First Come First Serve)
2. SJF غير الاستباقية (Shortest Job First)
3. Priority Scheduling غير الاستباقية
4. Round Robin بقيمة `Time Quantum = 2`

## هياكل البيانات المستخدمة

- Array لحفظ العمليات والنتائج ومقاطع مخطط Gantt.
- Singly Linked List لتخزين بيانات العمليات الأصلية.
- Circular Queue لخوارزميتي FCFS وRound Robin.
- Binary Min-Heap Priority Queue لخوارزميتي SJF وPriority.
- Stack لعرض سجل التنفيذ من الأحدث إلى الأقدم.

## المخرجات

ينفذ البرنامج الخوارزميات الأربع تلقائيًا ويعرض لكل خوارزمية:

- ترتيب تنفيذ العمليات.
- مخطط Gantt نصي.
- Completion Time (CT).
- Waiting Time (WT).
- Turnaround Time (TAT).
- متوسط زمن الانتظار ومتوسط زمن الدوران.
- وقت انشغال المعالج ووقت الخمول والزمن الكلي.
- CPU Utilization.

ثم يعرض جدول مقارنة نهائيًا بين الخوارزميات.

## التشغيل السريع

### Windows باستخدام MinGW g++

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic src/CPU_Scheduling_Simulator.cpp -o CPU_Scheduling_Simulator.exe
.\CPU_Scheduling_Simulator.exe
```

ويمكن تشغيل الملف `run_windows.bat` مباشرة من المجلد الرئيس.

### عرض النتائج أثناء المناقشة

يشغّل `START_DEMO_WINDOWS.bat` المصدر الحالي بعد تجميعه، ويحفظ نتائج التنفيذ الجديدة في `output/Live_Output.txt` ثم يفتحها في المفكرة لتسهيل التنقل بينها. يتطلب مترجم `g++` يدعم C++17 ومضافًا إلى `PATH`. عند فشل التجميع أو التنفيذ، يتوقف قبل فتح النتائج.

يتضمن [دليل العرض والتشغيل](docs/Presentation_and_Run_Guide_AR.pdf) ترتيب عرض مدته نحو ست دقائق، وخطوات التشغيل من Dev-C++، وشرح النتائج، ومواضع الكود المهمة، وأسئلة المناقشة المتوقعة. وتوجد [نسخة Word](docs/Presentation_and_Run_Guide_AR.docx) قابلة للتعديل.

### Linux أو macOS

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/CPU_Scheduling_Simulator.cpp -o CPU_Scheduling_Simulator
./CPU_Scheduling_Simulator
```

ويمكن تنفيذ الملف `run_linux.sh` من الطرفية.

### باستخدام CMake

```bash
cmake -S . -B build
cmake --build build
```

## بنية المستودع

```text
cpu-scheduling-simulation-ds/
├── .devcontainer/devcontainer.json
├── .github/workflows/run-project.yml
├── .vscode/tasks.json
├── scripts/verify_output.py
├── src/
│   └── CPU_Scheduling_Simulator.cpp
├── docs/
│   ├── Project_Report_AR.docx
│   ├── Project_Report_AR.pdf
│   ├── Run_Guide_AR.docx
│   ├── Run_Guide_AR.pdf
│   ├── Code_Explanation_AR.docx
│   ├── Code_Explanation_AR.pdf
│   ├── Presentation_and_Run_Guide_AR.docx
│   ├── Presentation_and_Run_Guide_AR.pdf
│   └── GitHub_Run_Guide_AR.md
├── output/
│   └── Sample_Output.txt
├── CMakeLists.txt
├── run_linux.sh
├── run_github.sh
├── run_windows.bat
├── START_DEMO_WINDOWS.bat
└── README.md
```

## العينة المدمجة

| Process | Arrival Time | Burst Time | Priority |
|---|---:|---:|---:|
| P1 | 2 | 5 | 3 |
| P2 | 3 | 2 | 1 |
| P3 | 4 | 8 | 4 |
| P4 | 5 | 3 | 2 |
| P5 | 7 | 4 | 1 |

الرقم الأصغر يمثل أولوية أعلى. تبدأ المحاكاة من الزمن صفر، وتكلفة تبديل السياق مفترضة بصفر.

## النتيجة المختصرة للعينة

| Algorithm | Average WT | Average TAT | CPU Utilization |
|---|---:|---:|---:|
| FCFS | 6.80 | 11.20 | 91.67% |
| SJF (Non-Preemptive) | 5.00 | 9.40 | 91.67% |
| Priority (Non-Preemptive) | 5.20 | 9.60 | 91.67% |
| Round Robin (q=2) | 8.40 | 12.80 | 91.67% |

التفاصيل الكاملة موجودة في مجلد `docs`، ونسخة المخرجات الفعلية موجودة في `output/Sample_Output.txt`.
