@echo off
rem build.cmd - the entry point to use on Windows.
rem
rem   .\gb\build.cmd                  build both binaries
rem   .\gb\build.cmd -Test            build and run every test
rem   .\gb\build.cmd -Test m03        run only tests whose name contains m03
rem   .\gb\build.cmd -Clean -Test     rebuild from scratch
rem   .\gb\build.cmd -Release         optimised build
rem
rem It exists because many Windows machines block .ps1 scripts by execution
rem policy. This wrapper runs build.ps1 with -ExecutionPolicy Bypass, so it
rem works without changing any machine setting.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %ERRORLEVEL%
