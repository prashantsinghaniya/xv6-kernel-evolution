\documentclass[12pt, a4paper]{article}
\usepackage[margin=1in]{geometry}
\usepackage{hyperref}
\usepackage{enumitem}

\title{Programming Assignment 03: \\ Scheduler-Aware Page Replacement in xv6}
\author{Uday Pratap Singh}
\date{CS3523: Operating Systems II}

\begin{document}

\maketitle

\section{Implemented Features}
\begin{itemize}[leftmargin=*]
    \item \textbf{Lazy Allocation:} Modified \texttt{uvmalloc} to increase the process size without eagerly allocating physical memory. Physical frames are allocated on-demand inside the \texttt{vmfault} handler.
    \item \textbf{Global Frame Table:} Implemented \texttt{frames\_table} to track physical page ownership, virtual addresses, and reference bits for all user processes.
    \item \textbf{Clock Page Replacement:} Implemented \texttt{evict\_page()} using a circular clock queue. It checks the hardware \texttt{PTE\_A} bit, provides a second chance by clearing the bit, and selects a victim when RAM is full.
    \item \textbf{Scheduler-Aware Eviction:} Integrated with the PA2 SC-MLFQ scheduler. The clock algorithm targets pages belonging to lower-priority processes (higher MLFQ queue index) before evicting pages from high-priority processes.
    \item \textbf{Swap Management:} Implemented a fixed-size 2D array in memory to act as swap space. Pages are copied to swap on eviction and restored via \texttt{memmove} on subsequent page faults.
    \item \textbf{Virtual Memory Statistics:} Added the \texttt{getvmstats} system call to accurately track \texttt{page\_faults}, \texttt{pages\_evicted}, \texttt{pages\_swapped\_in}, \texttt{pages\_swapped\_out}, and \texttt{resident\_pages} without metric leaks during process teardown.
\end{itemize}

\section{Design Decisions \& Assumptions}
\begin{itemize}[leftmargin=*]
    \item \textbf{TLB Flushing:} A critical design decision was to invoke \texttt{sfence.vma} immediately after modifying a victim's \texttt{PTE} during eviction. This flushes the Translation Lookaside Buffer, preventing stale cache accesses and fatal \texttt{kerneltrap} panics.
    \item \textbf{Swap Space Medium:} As per the assignment constraints, swap space is simulated using a pre-allocated kernel memory array (\texttt{swap\_space[SWAP\_SIZE][PGSIZE]}) rather than actual disk I/O.
    \item \textbf{Teardown Safety:} To prevent dangling pointers, \texttt{remove\_from\_frame\_table} is hooked into \texttt{uvmunmap} so that frames are properly decoupled from exiting processes before being returned to the \texttt{kalloc} free list.
\end{itemize}

\section{Experimental Results}
Three user-space test programs were developed to validate the memory subsystem. The system has a strict physical memory ceiling of 64 frames per process.

\subsection{Test Case 1: Baseline Stress Test (Thrashing)}
\textbf{Workload:} A single process lazily allocated 100 pages, wrote to them sequentially, and then read them back. \\
\textbf{Results:}
\begin{itemize}
    \item The initial write phase generated exactly 101 page faults and 57 evictions, successfully keeping the resident pages strictly at the 64-frame limit.
    \item The subsequent read phase forced the OS to swap in the evicted pages, resulting in a final count of 202 page faults and 158 evictions.
    \item \textbf{Conclusion:} Data verification passed with 0 errors, proving that the lazy allocation, swap-out, and swap-in logic perfectly maintain data integrity under heavy thrashing.
\end{itemize}

\subsection{Test Case 2: Scheduler-Aware Eviction (Priority Duel)}
\textbf{Workload:} Two processes simultaneously exerted memory pressure exceeding total physical RAM. Process A (VIP) spammed system calls to remain in the highest priority MLFQ queue. Process B (Victim) burned CPU time to drop to the lowest priority queue. Both allocated 35 pages (70 pages total). \\
\textbf{Results:}
\begin{itemize}
    \item The VIP process successfully faulted its 35 pages into RAM and suffered \textbf{0 evictions}.
    \item The Victim process suffered \textbf{23 evictions}.
    \item \textbf{Conclusion:} The clock algorithm successfully evaluated \texttt{owner->priority} during its sweep, effectively protecting the interactive process's working set by sacrificing the low-priority process's frames.
\end{itemize}

\subsection{Test Case 3: Clock Sweeper (Deadlock Prevention)}
\textbf{Workload:} Allocated exactly 43 pages to perfectly fill the remaining physical RAM up to the 63-page system capacity. Wrote to all pages to ensure all hardware \texttt{PTE\_A} bits were set to 1. Then, allocated exactly 1 additional page to force an eviction. \\
\textbf{Results:}
\begin{itemize}
    \item The OS successfully survived a 360-degree sweep of the frame table, clearing all reference bits to 0, and evicting the oldest page without freezing or panicking.
    \item \textbf{Conclusion:} The circular queue logic and reference-bit clearing mechanics in \texttt{evict\_page()} are mathematically sound and immune to infinite-loop deadlocks.
\end{itemize}

\end{document}
