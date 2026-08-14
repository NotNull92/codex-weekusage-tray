# Native Win32 release-candidate evidence

This record contains no account data, tokens, browser state, or user paths.

## Build and package

- The native implementation is unchanged in the final managed-cleanup commit. The release EXE is rebuilt from the exact final release commit immediately before upload.
- Commands passed: `native\build.cmd tests`, `native\out\CodexWeekUsageTrayTests.exe`, `native\build.cmd`, and `native\out\CodexWeekUsageTray.exe --self-test`.
- Release EXE: `CodexWeekUsageTray.exe`, 1,071,104 bytes.
- SHA-256 is generated from that final EXE and distributed only in the accompanying release `SHA256SUMS` manifest.
- Authenticode status: `NotSigned`.
- ZIP verification: `tar -tvf CodexWeekUsageTray-win-x64-v2.0.0.zip` listed exactly one file, `CodexWeekUsageTray.exe`.
- Packaged `--self-test` exited `0`.
- Packaged `--uninstall-dry-run` exited `0` and left the current-user notification-icon key count unchanged (62 before and after).

## Security scan

ESET command-line scan used `/clean-mode=none` on both the release folder and ZIP. Both exited `0`; the folder scan examined one object and the ZIP scan examined three objects. Both reported zero detections and zero cleaned objects.

## Memory sample

A short normal native-host sample, with no browser sign-in action, measured the processes separately:

| Process | Private memory | Working set |
| --- | ---: | ---: |
| Native tray host | 4.01 MiB | 11.24 MiB |
| Direct Codex App Server child | 39.01 MiB | 103.29 MiB |

The Codex child is required for account/OAuth protocol work and is not counted as native-host memory.

## UI and behavior coverage

Independent native visual QA previously passed both pending and ready panel captures, including the large lavender `--`/`73` tray rendering, real Win32/GDI controls, English copy, and the rightmost Close button. The rendering files (`popup.cpp`, `popup_paint.cpp`, and `tray_icon.cpp`) have not changed since that review. Native behavior is also covered by the strict test executable and release `--self-test` above.

## Release caveat

The EXE is intentionally unsigned. Public release publication must attach the ZIP and the matching SHA-256 manifest, then independently re-download and re-check the ZIP entry and hash.
