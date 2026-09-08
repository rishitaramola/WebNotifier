# 🌐 WebNotifier

WebNotifier is an automated website monitoring system designed to continuously check the availability and health of web resources. It replaces manual website checking with a high-concurrency C++ monitoring engine that detects failures, stores monitoring history, and helps users track website performance through a web dashboard.

---

## 💠 Tech Stack

▫️<b>Backend & Monitoring Engine : </b>C++<br>
▫️<b>Concurrency : </b>POSIX Threads (pthreads), Mutexes, Thread-Safe Queue<br>
▫️<b>Networking : </b>HTTP/REST, Socket Programming<br>
▫️<b>Frontend : </b>HTML5, CSS3, JavaScript<br>
▫️<b>Database : </b>Relational DBMS, SQL<br>
▫️<b>Scheduling : </b>Cron / OS Task Scheduler<br>
▫️<b>Reporting : </b>C++ PDF Generator<br>
▫️<b>Alerts : </b>SMTP / Email<br>

---

## 💠 Features

### ▫️Website Monitoring

> Automatically monitors registered websites and determines whether they are **UP or DOWN** without requiring manual checks.

### ▫️High-Concurrency Monitoring

> Uses a C++ multithreaded worker pool and POSIX threads to process multiple website monitoring tasks concurrently.

### ▫️Thread-Safe Task Queue

> A synchronized task queue safely transfers monitoring tasks from the scheduler to worker threads using mutex-based synchronization and prevents race conditions.

### ▫️Health Checks

> Monitors multiple health signals including HTTP status codes, SSL certificate validity/expiry, Time to First Byte (TTFB), keyword presence, error rates, and downtime.

### ▫️Database & Analytics

> Stores monitoring results and historical data in a relational database with transaction handling, SQL queries, normalization, and analytical data aggregation.

### ▫️Web Dashboard

> Provides a web interface for adding and managing URLs, viewing monitoring results, displaying charts, and monitoring system resources.

### ▫️Real-Time Alerts

> When a monitored website fails, the C++ monitoring engine communicates with the Alert Gateway to trigger an SMTP-based email notification.

### ▫️Weekly Reports

> A C++ background reporting process uses OS-level scheduling to generate weekly uptime and downtime analytics and produce PDF reports.

---

## 💠 How It Works

```text
Web Dashboard
      │
      ▼
C++ Backend API
      │
      ▼
Database
      │
      ▼
C++ Task Scheduler
      │
      ▼
Thread-Safe Task Queue
      │
      ▼
Multithreaded Worker Pool
      │
      ▼
HTTP / Network Health Check
      │
      ├──────────────► UP / DOWN Result
      │                       │
      │                       ▼
      │                   Database
      │
      └──────────────► Alert Gateway ───► SMTP Email
```

---

## 💠 Academic Contribution

### ▫️Operating Systems

> The project will demonstrate OS concepts including POSIX threads, multithreading, thread-safe queues, mutex synchronization, race-condition prevention, task scheduling, background process execution, Cron, socket programming, concurrent HTTP handling, and CPU/memory resource monitoring.

### ▫️DBMS

> The project will demonstrate relational schema design, normalization, primary and foreign keys, CRUD operations, transaction handling, monitoring-result insertion, SQL aggregation, analytical queries, and SQL Views.

---

## 💠 Team Members

### ▫️Rishita Ramola

> C++ Scheduler & Data Analytics

### ▫️Divyansh Sood

> C++ Backend API & Alert Gateway

### ▫️Shivank Garg

> C++ Concurrency Engine

### ▫️Simran Negi

> Web Dashboard & System Monitoring

---

## 💠 Expected Outcomes

> ⚡ Automated 24/7 website monitoring<br>
> 🧵 High-concurrency monitoring using a C++ worker pool<br>
> 🔒 Safe task synchronization and race-condition prevention<br>
> 🌐 Automated HTTP/network health checks<br>
> 🗄️ Reliable storage of monitoring results<br>
> 📊 Uptime and downtime analytics<br>
> 📧 Automated failure notifications<br>
> 📄 Weekly PDF monitoring reports<br>
> 🖥️ Web-based monitoring dashboard<br>

---

## ✨ Project Highlights

> 🚀 Automated website monitoring instead of manual checking<br>
> 🧵 Multithreaded C++ monitoring engine<br>
> 🔒 Thread-safe task queue using synchronization<br>
> 🌐 HTTP and network health monitoring<br>
> 🗄️ Relational database integration<br>
> 📊 Historical uptime and downtime analytics<br>
> 📧 SMTP-based failure alerts<br>
> 📄 Automated weekly PDF reports<br>
> 🖥️ Web dashboard for monitoring and visualization<br>
> 💻 Practical integration of Operating Systems and DBMS concepts<br>
