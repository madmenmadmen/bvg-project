@echo off
setlocal
set "ROOT=%~dp0\bvg-java\bvg-cli"
set "JAR=%ROOT%\build\libs\bvg-java-cli.jar"
if not exist "%JAR%" (
    echo bvg-java not built: %JAR%
    echo run: cd /d "%ROOT%" ^&^& gradlew cliJar
    exit /b 1
)
java -jar "%JAR%" %*