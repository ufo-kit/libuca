@echo Please run this from a Visual Studio Console
@echo Building to %1
@echo Installing and deploying dependencies
conan install . -pr:a conan/windows_release --build=missing --deployer=runtime_deploy --deployer-folder=%1/bin
@if %errorlevel% neq 0 exit /b %errorlevel%

@echo Configuring CMake
cmake --preset conan-release -DCMAKE_INSTALL_PREFIX=%1
@if %errorlevel% neq 0 exit /b %errorlevel%

@echo Building
cmake --build --preset conan-release --target install
