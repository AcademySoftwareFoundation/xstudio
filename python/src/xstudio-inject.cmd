@echo off
set PYTHONPATH=%~dp0python3\Lib\site-packages
"%~dp0python3\python.exe" -m xstudio.cli.inject %*
