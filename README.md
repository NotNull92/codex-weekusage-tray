# Codex WeekUsage Tray

Windows 알림 영역(시스템 트레이)에서 Codex의 7일 사용량을 확인하는 작은 앱입니다. `W --` 아이콘을 클릭하면 사용량, 초기화 시각, 초기화까지 남은 시간을 볼 수 있습니다.

이 앱은 작업표시줄의 시간·날짜 영역 위에 창을 띄우지 않습니다. 표준 Windows 트레이 아이콘이므로 Windows 설정의 **기타 시스템 트레이 아이콘**에서 이 앱을 켤지 여부는 사용자가 결정합니다. 아이콘 위에 마우스를 올리면 `W 73% · 7일 잔여`처럼 정확한 잔여 비율이 표시됩니다.

## 요구 사항

- Windows 10 또는 Windows 11
- 설치된 [Codex CLI](https://developers.openai.com/codex/cli/)
- 소스에서 실행할 경우 .NET 10 SDK

처음 실행할 때 Codex가 ChatGPT 계정으로 로그인되어 있지 않으면 트레이 팝업 또는 우클릭 메뉴의 **Codex 로그인**을 누르세요. 앱은 Codex가 제공한 HTTPS 인증 페이지를 기본 브라우저로 열며, 비밀번호·API 키·토큰을 직접 받거나 저장하지 않습니다. Codex CLI가 API 키만으로 실행 중이면 ChatGPT 로그인이 필요합니다.

## 실행

```powershell
dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj
```

Windows 알림 영역에서 `W --` 아이콘을 찾으세요. 보이지 않으면 Windows 설정의 **개인 설정 > 작업 표시줄 > 기타 시스템 트레이 아이콘**에서 `Codex WeekUsage Tray`를 켭니다. 7일 제한 버킷을 읽지 못한 경우에도 팝업에서 **로그인** 또는 **다시 확인**을 선택할 수 있습니다.

## 동작

- `account/rateLimits/updated` 알림을 받으면 즉시 갱신합니다.
- 알림 누락에 대비해 5초마다 한 번만 다시 확인합니다. 1분 단위 폴링을 사용하지 않습니다.
- 사용량을 읽기 전에는 `W --`, 읽은 뒤에는 `W73`처럼 잔여 비율을 표시한 트레이 아이콘을 클릭하면 7일 잔여·사용 비율, 초기화 시각, 남은 시간이 나타납니다.
- 팝업의 **새로 고침**을 누르면 즉시 다시 조회합니다.
- 로그인이 없으면 팝업의 **로그인** 또는 우클릭 메뉴의 **Codex 로그인**이 브라우저 인증을 시작합니다.
- 아이콘을 우클릭하면 상태 보기, 새로 고침, 로그인, 종료 메뉴를 사용할 수 있습니다.

## 배포용 빌드

다른 Windows x64 사용자가 .NET을 별도 설치하지 않고 실행할 수 있는 폴더를 만듭니다.

```powershell
dotnet publish src/CodexWeekUsageTray/CodexWeekUsageTray.csproj `
  -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true -o artifacts/win-x64
```

생성된 `artifacts/win-x64/CodexWeekUsageTray.exe`를 실행하면 됩니다. Codex CLI 자체는 각 사용자 컴퓨터에 있어야 합니다. 로그인되지 않은 경우 앱에서 브라우저 로그인을 시작할 수 있습니다.

## 개인정보

이 저장소에는 계정 이름, API 키, 액세스 토큰, 실제 사용량을 포함하지 않습니다. 앱은 Codex App Server가 반환한 현재 세션의 제한 정보만 화면에 표시하며 파일이나 원격 서버에 저장하지 않습니다.

## 개발 확인

```powershell
dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj
dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -- --self-test
```
