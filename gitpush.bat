@echo off
cd /d D:\WSPR-Sked-2 || (echo Folder D:\WSPR-Sked-2 not found & pause & exit /b 1)
git add -A
echo.
echo Changes to be committed:
git status --short
echo.
set "MSG="
set /p MSG=What changed (blank to cancel)? 
if "%MSG%"=="" (echo Cancelled - nothing committed & pause & exit /b 0)
git commit -m "%MSG%" || (echo Nothing committed & pause & exit /b 1)
git push
echo.
pause