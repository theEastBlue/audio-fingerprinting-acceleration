# Meeting Minutes

## Meeting Information

**Meeting Date/Time:** 27 July 2026, 4:00pm – 9:00pm  
**Meeting Purpose:** Lab session to review project progress, discuss report requirements, and plan the next development tasks.  
**Meeting Location:** In-person Lab Session  
**Note Takers:** Esha and Venus  

## Attendees

People who attended:

* Esha
* Venus

## Agenda Items

| Item                    | Description                                                                                                                                                        |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Project progress review | • Review current implementation status and discuss remaining milestones.<br>                      |
| Report guidance         | • Receive advice from tutors on report structure, performance evaluation, and future work.<br> |
| Development planning    | • Allocate remaining implementation and optimisation tasks before the next meeting.     |                                                                           |

## Discussion Items

| Item                      | Notes                                                                                                                                                                                                                                                                |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tutor feedback on report  | Chia Wei and Tony provided guidance on the final report. They emphasised that the report is the most important deliverable and should clearly demonstrate the progression of the project from the original baseline through to the accelerated implementation.       |
| Report structure          | Agreed to organise the report into the following sections: **Baseline**, **Minimum Viable Product (MVP)**, **Results (Acceleration/Deceleration with justification)**, and **Future Work**. Any performance improvements or regressions should be clearly explained. |
| Performance analysis      | Tutors recommended timing each major function in the software baseline to identify the largest performance bottleneck. This information will be used with Amdahl's Law to justify which component should be accelerated next as future extended work (likely the FFT stage).                 |
| Overhead computation      | Discussed measuring accelerator overhead by subtracting kernel execution time from the total latency. This will quantify communication and data transfer costs between the processor and FPGA.                                                           |
| Future optimisation ideas | Potential future optimisations include kernel fusion to reduce overhead, improving throughput, and determining whether it is primarily compute-bound or memory-bound.                                                                                   |
| Audio processing          | Discussed audio resizing approaches with Chia Wei to ensure feasibility and compatibility with the fingerprint pipeline.                                                                                                                                                             |

## Action Items

| Done? | Item                                                                                                                                            | Responsible | Due Date |
| ----- | ----------------------------------------------------------------------------------------------------------------------------------------------- | ----------- | -------- |
|       | Replace `fft_radix2` with the FFT implementation from the textbook and replace the the diamond/Manhattan-distance neighborhood scan functionality with a lower-latency alternative. | Venus       | 31 Jul   |
|       | Create a testbench and separate the code into host and kernel components to enable execution on the Kria board.                          | Esha        | 31 Jul   |

## Other Notes & Information

* Benchmarking should continue using the same baseline metrics throughout the project to ensure fair comparisons.
* Future acceleration should be justified using Amdahl's Law and supported with measured performance data (e.g., FFTs per second).
* Report discussion should include both successful optimisations and any attempted approaches that did not improve performance, together with explanations.
* Kernel fusion remains a possible optimisation if time permits.

**Next Meeting:** Friday, 31 July 2026, 1:00pm
