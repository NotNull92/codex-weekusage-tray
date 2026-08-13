# Codex WeekUsage Tray

Windows 작업표시줄 오른쪽에 Codex의 7일 사용량 잔여 비율을 항상 표시하는 작은 앱입니다. 카운터를 클릭하면 사용량, 초기화 시각, 초기화까지 남은 시간을 볼 수 있습니다.

Windows는 임의의 텍스트를 작업표시줄에 직접 추가하는 공식 API를 제공하지 않습니다. 이 앱은 기본 작업표시줄 위에 작은 클릭 가능 창을 배치하는 방식으로 `W 73%` 같은 카운터를 표시합니다.

## 요구 사항

- Windows 10 또는 Windows 11
- 설치 및 로그인된 [Codex CLI](https://developers.openai.com/codex/cli/)
- 소스에서 실행할 경우 .NET 10 SDK

Codex CLI가 API 키만으로 실행 중이면 제한 정보를 읽을 수 없습니다. ChatGPT 계정으로 로그인된 Codex CLI 세션이 필요합니다.

## 실행

```powershell
dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj
```

작업표시줄 오른쪽의 `W --`가 `W <잔여>%`로 바뀌면 준비된 것입니다. 7일 제한 버킷을 계정에서 제공하지 않으면 팝업에 안내 문구를 표시합니다.

## 동작

- `account/rateLimits/updated` 알림을 받으면 즉시 갱신합니다.
- 알림 누락에 대비해 5초마다 한 번만 다시 확인합니다. 1분 단위 폴링을 사용하지 않습니다.
- 카운터를 클릭하면 7일 잔여·사용 비율, 초기화 시각, 남은 시간이 나타납니다.
- 팝업의 **새로 고침**을 누르면 즉시 다시 조회합니다.

## 배포용 빌드

다른 Windows x64 사용자가 .NET을 별도 설치하지 않고 실행할 수 있는 폴더를 만듭니다.

```powershell
dotnet publish src/CodexWeekUsageTray/CodexWeekUsageTray.csproj `
  -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true -o artifacts/win-x64
```

생성된 `artifacts/win-x64/CodexWeekUsageTray.exe`를 실행하면 됩니다. Codex CLI 자체와 해당 CLI의 로그인 세션은 각 사용자 컴퓨터에 있어야 합니다.

## 개인정보

이 저장소에는 계정 이름, API 키, 액세스 토큰, 실제 사용량을 포함하지 않습니다. 앱은 Codex App Server가 반환한 현재 세션의 제한 정보만 화면에 표시하며 파일이나 원격 서버에 저장하지 않습니다.

## 개발 확인

```powershell
dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj
dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -- --self-test
```
