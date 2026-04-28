echo Start build SRT Router time: %date% %time%

set curDir=%~dp0

cd %curDir%\SrtRouter
call "CreateVSProj.bat"

:: Check if CreateVSProj.bat failed
if %errorlevel% NEQ 0 (
    echo call CreateVSProj.bat with error level %ERRORLEVEL%
    goto build_failed
)

set buildType=Release

:: Build the solution with Release configuration for x64
::devenv SrtRouter.sln /Build "%buildType%|x64"
msbuild SrtRouter.sln /p:Configuration=%buildType% /p:Platform=x64

:: Check if the build failed
if %errorlevel% NEQ 0 (
    echo Build failed with error level %ERRORLEVEL%
    goto build_failed
)

:: Build and install the project
::devenv SrtRouter.sln /Build "%buildType%|x64" /project INSTALL
msbuild INSTALL.vcxproj /p:Configuration=%buildType% /p:Platform=x64

:: Check if the installation step failed
if %errorlevel% NEQ 0 (
    echo Build install failed with error level %ERRORLEVEL%
    goto build_failed
)

:: If successful, echo the result
echo Build successful

echo Package to zip
set buildBin=%curDir%\SrtRouter\build\bin
set /P ver=<%buildBin%\version.properties
echo ver=%ver%
if "%ver%" == "" (
set ver=Internal-1.0.0.0
)
set setupDir=%curDir%
echo setupDir: %setupDir%
:: mkdir %setupDir%

cd %curDir%
set tempDir=%curDir%temp
set srtrouterDir=%tempDir%\SrtRouter
if exist %srtrouterDir% (
rd /s/q %srtrouterDir%
)
mkdir %srtrouterDir%

copy /Y %buildBin%\version.properties %srtrouterDir%
set srtDir=%curDir%SrtRouter\build\bin
copy /Y %srtDir%\SrtRouter.exe %srtrouterDir%
copy /Y %srtDir%\srtrouterconfig.json %srtrouterDir%
copy /Y %srtDir%\srt.dll %srtrouterDir%
copy /Y %srtDir%\libssl-1_1-x64.dll %srtrouterDir%
copy /Y %srtDir%\libcrypto-1_1-x64.dll %srtrouterDir%
copy /Y %srtDir%\srtrouter.readme.md %srtrouterDir%
copy /Y %srtDir%\version.properties %srtrouterDir%

cd %srtrouterDir%
tar -vczf %setupDir%\SrtRouter.%ver%.tgz *.*
cd %curDir%

:: Check if the package step failed
if %errorlevel% NEQ 0 (
    echo Build package failed with error level %ERRORLEVEL%
    goto build_failed
)

goto build_end

:build_failed
:: Log the end time and indicate failure
echo End build SRT Router time: %date% %time%
cd %curDir%
exit /b 1

:build_end
:: Log the end time
echo End build SRT Router time: %date% %time%
cd %curDir%
exit /b 0
