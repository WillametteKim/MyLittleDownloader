 My Little Downloader(MLD)의 사용성을 강화하기 위한 애드온입니다.<br><br>
# Add_on #1. Catch 
### 개요
사용자가 일일이 파일 링크를 복사하여 프로그램에 붙여넣기가 귀찮을 수 있습니다.<br>
한꺼번에 많은 파일을 받는 헤비 다운로더라면 귀찮음은 배가 되죠. 나의 링크, 알아서 MLD로 던질 수 없을까요?
<br>
### 사용법
catcha를 사용하기 위해서 외부 모듈을 설치해야 합니다.<br>
```
$ pip install pyperclip
```
이후 catcha를 MLD와 같은 경로에 위치 후 실행하면, 이제 catcha가 클립보드를 확인할 겁니다!<br>
만약 http로 시작하는 URL이 클립보드에 담겨있다면, 나머지는 catcha가 알아서 해줄 겁니다. 

<br><br>
# Add_on #2. SimpleHTTPServerUpload with Korean

### 개요
가장 쉽고 간단하게 HTTP 기반 웹 서버를 구동하는 방법은 파이썬 내장 모듈을 사용하는 것입니다. 
```
$ python -m http.server [PortNumber]
```

그러나 기본 모듈에선
- **한글**에 대한 문자 인코딩을 해주지 않아 한글 파일이 깨집니다. 
- **파일 업로드**를 지원하지 않습니다.
- **폴더 다운로드**를 지원하지 않습니다.

이제 SimpleHTTPServerUpload with Korean과 함께 한 단계 진보한 HTTP 웹 서버를 운영해보세요. 
<br>
### 사용법
실행 문법은 기존 SimpleHTTPServer와 동일합니다.<br>
웹 서버를 구동할 디렉토리에 파이썬 스크립트를 위치한 뒤, 명령어를 이용해 실행해주세요.<br>
만약 시스템 시작 시 같이 구동시키고 싶다면 
- 리눅스에선 rc.local 수정을
-  윈도에선 작업 스케쥴러를

수정하여 설정 가능합니다.
<br>
### 이슈 
- 한글 디렉토리에 대해선 파일 다운이 불가합니다.
- 공백은 '+'로 대체됩니다.
<br>

### 사용 모습
- [한글 지원]

>![korean](./img/korean.png)
<br>

- [디렉토리 다운]

>![zip](./img/zip.png)
<br>

- [업로드]

>![upload](./img/upload.png)

<br><br>

# 경고
모든 애드온은 **파이썬 3.6** 이상의 환경에서 실행이 검증되었습니다. 
