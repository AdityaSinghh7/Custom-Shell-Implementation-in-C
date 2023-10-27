# Custom-Shell-Implementation-in-C

Developed a custom Unix-like shell utility to seamlessly handle process management and job control, empowering users with sophisticated tools for multitasking and process control directly from the command line. This terminal-based application offers:

**Foreground & Background Process Execution:** Launch applications or commands in both foreground and background modes, providing real-time user feedback.
**Interactive Job Management: **Use a combination of signals (e.g., SIGTSTP, SIGINT, SIGCHLD) to control processes, allowing users to stop, resume, or terminate tasks as needed.
**Job List Monitoring:** View a detailed list of active jobs, their states, and associated command lines.
**I/O Redirection:** Reroute standard input and output for commands, supporting both overriding and appending modes.
**Directory Navigation:** Incorporate built-in cd and pwd commands for easy directory traversal.
**Rich Signal Handling:** Interpret and manage multiple Unix signals to regulate process behavior and ensure smooth execution.
**Technologies Used:** C, Unix System Calls, Process Management, Signal Handling
