# cpu_usage_tracker

4 wątki:
-Reader
-Analyzer
-Printer
-Watchdog

Przechwytywanie SIGTERM

(kompilacja clang - wyłączone ostrzeżenia dla VLA (użyłem VLA) i macro-expansion (użycie .sa_handler daje warning))