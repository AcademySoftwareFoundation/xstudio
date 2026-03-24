@echo off
set PYTHONPATH=%~dp0python3\Lib\site-packages
set PYTHONSTARTUP=%~dp0python3\Lib\site-packages\xstudio\cli\xstudiopy_startup.py
"%~dp0python3\python.exe" %*
