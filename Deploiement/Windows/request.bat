set VERSION="0.1.0"

set EXE_NAME="./DNAISetup/DNAISetup.exe"

set CURL_CMD="C:\Users\Victor\Downloads\curl-7.59.0-win64-mingw\bin\curl.exe"
set JQ_CMD="C:\Users\Victor\Downloads\jq-win64.exe"
set SLUG=installer

@echo off
set /p login="Enter login: "
set /p password="Enter password: "
@echo on

%CURL_CMD% -X POST https://api.preprod.dnai.io/signin -H "authorization: Basic YWRtaW46ZG5haQ=="  -H "content-type: application/json" -d "{ \"login\": \"%login%\", \"password\": \"%password%\"}" > tmpSIGNIN
set /p contents= < tmpSIGNIN
DEL tmpSIGNIN

(echo %contents% | %JQ_CMD% -r ".token") > tmpTOKEN
set /p token= < tmpTOKEN
DEL tmpTOKEN

echo %token%

%CURL_CMD% -X POST https://api.preprod.dnai.io/download/softwares/windows -H "authorization: Bearer %token%" -H "content-type: application/json"  -d "{\"title\": \"Revision for %VERSION%\", \"description\": \"Generated by installer.sh on Windows.\", \"currentVersion\": \"%VERSION%\", \"slug\": \"%SLUG%\"}" > tmpSIGNIN
set /p contentsWindows= < tmpSIGNIN
DEL tmpSIGNIN

(echo %contentsWindows% | %JQ_CMD% -r "._id") > tmpTOKEN
set /p id= < tmpTOKEN
DEL tmpTOKEN

%CURL_CMD% -X PATCH https://api.preprod.dnai.io/download/softwares/windows/%SLUG% -H "authorization: Bearer %token%" -H "content-type: application/json"  -d "{\"title\": \"Revision for %VERSION%\", \"description\": \"Generated by installer.sh on Windows.\", \"currentVersion\": \"%VERSION%\",  \"slug\": \"%SLUG%\"}" > tmpSIGNIN
set /p contentsWindows= < tmpSIGNIN
DEL tmpSIGNIN

(echo %contentsWindows% | %JQ_CMD% -r "._id") > tmpTOKEN
set /p id= < tmpTOKEN
DEL tmpTOKEN

echo %id%

%CURL_CMD% -X PUT --upload-file %EXE_NAME%  https://api.preprod.dnai.io/download/softwares/windows/%SLUG%/%VERSION% -H "authorization: Bearer %token%" -H "content-type: text/plain" 

pause