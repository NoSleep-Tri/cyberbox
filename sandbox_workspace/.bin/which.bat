@for %%i in (%*) do @(where %%i 2>nul || where %%i.bat 2>nul || where %%i.exe 2>nul || where %%i.cmd 2>nul)
