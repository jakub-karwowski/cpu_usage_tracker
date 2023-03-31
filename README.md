# cpu_usage_tracker

This program reads `/proc/stat` and calculates cpu usage using this [formula](https://stackoverflow.com/questions/23367857/accurate-calculation-of-cpu-usage-given-in-percentage-in-linux).
- 4 threads: reader, analyzer, printer, watchdog
- `SIGTERM` capture
