; orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
; SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
;
; The installer's hooks. Once orior is installed it asks whether errors file on their own as issues on
; dstroy0/orior, yes the answer given by default and the answer a silent install takes, and keeps the
; answer in orior's own folder, %APPDATA%\orior\settings.json, where the window and the command line
; read it (src/ui/cli/src/report.rs). An install per user, the bundle's own mode, keeps it for that
; user.

!macro NSIS_HOOK_POSTINSTALL
  Push $R0
  Push $R1
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON1 "orior files the errors it meets as issues on dstroy0/orior on its own: through your GitHub CLI where it is signed in, else as a page opened for you to submit. Help, Automatic Error Reports turns it on or off later.$\r$\n$\r$\nFile them?" /SD IDYES IDNO orior_reports_off
  StrCpy $R0 "true"
  Goto orior_reports_kept
  orior_reports_off:
  StrCpy $R0 "false"
  orior_reports_kept:
  CreateDirectory "$APPDATA\orior"
  FileOpen $R1 "$APPDATA\orior\settings.json" w
  FileWrite $R1 '{$\r$\n  "auto_report": $R0$\r$\n}$\r$\n'
  FileClose $R1
  Pop $R1
  Pop $R0
!macroend
