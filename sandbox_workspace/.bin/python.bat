@echo off
if exist "%USERPROFILE%\.local\bin\python3.12.exe" (
  "%USERPROFILE%\.local\bin\python3.12.exe" %*
) else if exist "C:\Users\Tri\.local\bin\python3.12.exe" (
  "C:\Users\Tri\.local\bin\python3.12.exe" %*
) else (
  python.exe %*
)
