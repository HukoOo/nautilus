# Nautilus 46.4 포크 변경 사항

- 기반: GNOME Nautilus 46.4 (GTK 4.14, libadwaita 1.5), Ubuntu 24.04 / GNOME 46 / X11
- 브랜치: `split-view` (아직 커밋하지 않음)
- 규모: 25개 파일, +1678 / −181줄, 스크립트 2개 추가
- 앱 ID: `org.gnome.NautilusDevel` (Devel 프로필이라 시스템 Nautilus와 따로 실행됨)

## 빌드와 실행

| 스크립트 | 하는 일 |
|---|---|
| `dev-run.sh` | 필요하면 `meson setup build -Dprofile=Devel ...`을 하고 `ninja -C build`로 빌드한 뒤 새 창을 엽니다. `--build-only`를 붙이면 빌드만 합니다. |
| `launch.sh` | 빌드 없이 실행만 합니다. 독 런처가 이 스크립트를 씁니다. 로그는 `~/.cache/nautilus-dev.log`에 남습니다. |

새 GSettings 키와 enum 값이 있어서 포크의 스키마가 필요합니다. 두 스크립트 모두 `GSETTINGS_SCHEMA_DIR=build/data`를 설정합니다.

---

## 1. 분할 보기 (Terminator 방식의 패널 트리)

창 하나를 여러 패널로 나눌 수 있습니다. 패널마다 자기 탭 바와 탭을 가집니다. 패널 안을 다시 나누면 GtkPaned가 중첩되면서 트리 구조가 됩니다.

**조작**

| 동작 | 단축키 | 액션 |
|---|---|---|
| 분할 켜기/끄기 (분할 중이면 현재 패널만 남김) | F3 | `win.toggle-split-view` |
| 위아래로 나누기 | Ctrl+Shift+O | `win.split-pane('horizontal')` |
| 좌우로 나누기 | Ctrl+Shift+E | `win.split-pane('vertical')` |
| 현재 패널 닫기 | Ctrl+Shift+W | `win.close-pane` |

- 빈 공간을 우클릭하면 "분할 보기(S_plit View)" 하위 메뉴가 있습니다.
- `horizontal`/`vertical`의 의미는 Terminator를 따릅니다. horizontal은 위아래, vertical은 좌우입니다.
- 클릭하거나 포커스가 들어간 패널이 활성 패널이 됩니다. 툴바, 경로 표시줄, 사이드바는 활성 패널을 따라갑니다.
- 분할 상태에서는 활성 패널 위쪽에 강조색 2px 표시줄이 나타납니다.
- 패널의 마지막 탭을 닫으면 그 패널은 자동으로 사라집니다.

**구현** (`nautilus-window.c`, `nautilus-window.ui`)
- 템플릿 콘텐츠는 `AdwBin pane_root`입니다. 패널은 `create_pane()`이 코드로 만듭니다(GtkBox `.nautilus-pane` = 표시줄 + AdwTabBar + AdwTabView).
- 구조: `GList *panes`, `split_pane()`, `remove_pane()`, `replace_in_pane_tree()`, `collapse_empty_panes()`(idle에서 실행).
- `window->tab_view`/`tab_bar`는 항상 첫 패널을 가리키고 `update_panes()`가 갱신합니다. 탭을 다루는 코드는 `get_tab_view_for_slot()`, `get_active_tab_view()` 등의 도우미를 거칩니다.
- 공개 API로 `nautilus_window_get_tab_view_for_slot()`을 추가했고, `nautilus-window-slot.c`가 이를 씁니다.
- 활성 패널 감지는 캡처 단계의 GtkGestureClick과 GtkEventControllerFocus로 합니다.
- 기타: `style.css`(패널 표시줄), `help-overlay.ui`(단축키 도움말), `nautilus-files-view-context-menus.ui`(우클릭 메뉴).

## 2. 확대 단계 확장

| 보기 | 기존 | 변경 후 (px) |
|---|---|---|
| 아이콘(그리드) | 4단계 | 32 / 48 / 64 / 96 / 168 / 256 / 384 / 512 (EXTRA_SMALL … HUGE, GIGANTIC) |
| 목록 | 3단계 | 16 / 32 / 64 / 96 / 128 |

- `nautilus-enums.h`와 gschema의 enum을 늘렸습니다.
- 큰 아이콘에서 썸네일이 흐려지지 않게, 썸네일 생성 크기를 한 단계 올렸습니다(배율 1에서 XLARGE). 관련 파일은 `nautilus-thumbnails.c`, `nautilus-directory-async.c`(`max_thumbnail_size`)입니다.
- `nautilus-list-base.c`와 `nautilus-view-cell.c`에서 icon-size 속성의 최댓값을 GIGANTIC으로 올렸습니다.

## 3. 아이콘 크기 슬라이더

- 앱 메뉴에 "아이콘 크기" 줄을 추가했습니다: [축소] 슬라이더 [확대]. 슬라이더는 로그 눈금이고, 값은 "N px"로 표시되며, 각 확대 단계 위치에 눈금이 있습니다.
- 확대 단계 사이의 크기도 자유롭게 고를 수 있습니다. 확대/축소 버튼과 단축키는 단계 크기 단위로 움직입니다.
- 설정 키: `icon-view icon-size`, `list-view icon-size` (uint, 0이면 확대 단계 값을 씀).
- 구현:
  - `NautilusViewInfo.zoom_icon_sizes`와 vfunc `set_icon_size`를 추가했습니다.
  - 공개 함수: `nautilus_list_base_get/set_icon_size`, `nautilus_files_view_get/set_icon_size`, `nautilus_files_view_get_zoom_icon_sizes`.
  - 그리드/목록 보기는 `icon_size`를 저장하고, 확대 단계는 그 값에서 계산합니다(가장 가까운 아래 단계). `default-zoom-level`도 같이 맞춥니다.
  - 창 쪽: `sync_icon_size_scale()`, `on_icon_size_scale_changed()`.
- 상태: 빌드는 되지만, 화면에서 직접 조작해 보는 확인은 아직 하지 않았습니다.

## 4. 글자 크기 슬라이더

- 환경설정 → "모양(Appearance)" 그룹에 글자 크기 슬라이더(50–200%)와 초기화 버튼을 추가했습니다.
- 설정 키: `preferences font-scale` (double, 0.5–2.0).
- `nautilus-application.c`의 CSS 제공자가 `.nautilus-grid-view`, `.nautilus-list-view`, `placessidebar`에 글자 크기를 적용합니다.
- 상태: 동작을 확인했습니다.
- 알려진 문제: 작은 아이콘에 큰 글자를 쓰면 그리드의 파일 이름이 좁은 칸에서 여러 줄로 꺾입니다.

## 5. 비모달 대화상자

| 창 | 변경 |
|---|---|
| 환경설정 | `gtk_window_set_modal(FALSE)` |
| 속성 | `nautilus-properties-window.ui`에서 `modal=False` |

이제 이 창들을 띄운 채로 탐색기 창을 계속 쓸 수 있고, 창을 끌어도 부모 창이 따라오지 않습니다. 다른 대화상자(연결 프로그램 선택, 압축, 새 폴더, 일괄 이름 바꾸기, 충돌)는 그대로 모달입니다.

## 6. 창 위치 저장/복원 (X11)

- 설정 키: `window-state initial-position` (ii), 기본값 (-1, -1).
- 최대화 상태가 아닐 때 `nautilus_window_save_geometry()`에서 위치를 저장합니다.
- 첫 번째 창이 열릴 때만 복원합니다. realize 시점에 USPosition 힌트를 넣고 XMoveWindow로 옮깁니다.
- 창 중심이 어느 모니터 안에도 없으면 복원하지 않습니다. 창 중심을 기준으로 삼은 이유는 그림자 때문에 x가 음수(예: -13)가 될 수 있기 때문입니다.
- `src/meson.build`: `dependency('x11', required: false)`를 추가했습니다. 도우미 함수는 `get_x11_position`, `is_position_on_a_monitor`, `is_only_window`, `restore_x11_position`입니다.

## 7. 다시 실행했을 때 창이 안 보이던 문제

**증상:** 마지막 창을 닫은 직후 독에서 다시 실행하면 창이 다른 창 뒤에 숨거나 보이지 않았습니다.

**원인과 수정** (`nautilus-application.c`)

1. 원본 Nautilus는 마지막 창을 닫은 뒤에도 프로세스가 12초 동안 남아 있습니다. 그 사이에 실행하면 요청이 그 프로세스로 넘어가는데, 이때 포커스 정보가 함께 넘어가지 않았습니다.
   → 마지막 창이 닫히면 비활성 타임아웃을 0으로 줄여 바로 종료합니다(`window_removed`). 창이 새로 생기면 12000으로 되돌립니다(`window_added`).
2. 서비스 모드로 떠 있을 때(예: "폴더에서 보기")는 프로세스가 계속 살아 있습니다. 그런데 GTK 4.14는 라이브러리를 불러오는 순간 `DESKTOP_STARTUP_ID`를 지워 버려서, 요청을 넘길 때 startup ID가 빠졌습니다.
   → `launch.sh`가 값을 `NAUTILUS_LAUNCH_STARTUP_ID`로 복사해 둡니다. `handle_local_options()`가 이 값을 `startup-id` 옵션으로 넘기고 환경 변수는 지웁니다. 받는 쪽은 `command_line`에서 `apply_startup_id()`를 호출해 `gtk_window_set_startup_id()`를 적용하고, ID에 담긴 `_TIME` 타임스탬프로 창을 앞으로 띄웁니다.

## 8. 우클릭 메뉴: 터미널 / Cursor에서 열기

| 우클릭 위치 | 메뉴 | 액션 | 표시 조건 |
|---|---|---|---|
| 빈 공간 | 터미널에서 열기 (Open in _Terminal) | `view.current-directory-terminal` | 현재 폴더가 로컬 경로일 때 |
| 빈 공간 | Cursor에서 열기 (Open in C_ursor) | `view.current-directory-cursor` | 로컬 경로이고 `cursor`가 PATH에 있을 때 |
| 폴더 1개 선택 | 터미널에서 열기 | `view.terminal` | 로컬 폴더일 때 |
| 항목 1개 선택 | Cursor에서 열기 | `view.cursor` | 로컬 파일/폴더이고 `cursor`가 있을 때 |

- 터미널: `terminator --working-directory=<경로>`로 엽니다. terminator가 없으면 `gnome-terminal`, 그것도 없으면 `x-terminal-emulator`를 해당 폴더에서 실행합니다.
- Cursor: `cursor <경로>`로 엽니다. 파일을 고르면 그 파일을, 폴더를 고르면 그 폴더를 엽니다.
- 원본의 "콘솔에서 열기"는 GNOME Console의 D-Bus 서비스가 있어야 보이는데, Ubuntu에는 그 서비스가 없어서 숨겨져 있었습니다. 시스템의 gnome-terminal 확장도 개발판은 불러오지 않습니다(확장 디렉터리가 다름). 그래서 새 액션을 따로 만들었습니다.
- 개발판의 `GSETTINGS_SCHEMA_DIR`는 실행하는 프로그램에 넘기지 않습니다.
- 구현: `nautilus-files-view.c`의 `spawn_in_directory()`, `open_terminal_at()`, `open_cursor_at()`. 활성화 조건은 `real_update_actions_state()`에서 정합니다. 메뉴는 `nautilus-files-view-context-menus.ui`에 있습니다.

---

## 시스템 연동 (저장소 밖)

| 항목 | 내용 |
|---|---|
| `~/.local/share/applications/org.gnome.NautilusDevel.desktop` | "Files (Dev)", `Exec=launch.sh --new-window %U`, `MimeType=inode/directory;` |
| 독 즐겨찾기 | `org.gnome.Nautilus.desktop`을 `org.gnome.NautilusDevel.desktop`으로 바꿈 |
| 기본 폴더 열기 프로그램 | `xdg-mime default org.gnome.NautilusDevel.desktop inode/directory` |
| `~/.local/share/dbus-1/services/org.freedesktop.FileManager1.service` | `Exec=launch.sh --gapplication-service` ("폴더에서 보기" 등) |
| 압축 파일 | file-roller 설치, 압축 형식 24개의 기본 프로그램을 `org.gnome.FileRoller.desktop`으로 지정 |

**원래대로 되돌리기**
```bash
gsettings set org.gnome.shell favorite-apps "$(gsettings get org.gnome.shell favorite-apps | sed 's/org.gnome.NautilusDevel.desktop/org.gnome.Nautilus.desktop/')"
xdg-mime default org.gnome.Nautilus.desktop inode/directory
rm ~/.local/share/dbus-1/services/org.freedesktop.FileManager1.service
```

## 확인이 남은 것

- 아이콘 크기 슬라이더를 화면에서 직접 조작해 보기
- 독에서 실행했을 때 창 위치가 복원되는지 처음부터 끝까지 확인
- 속성 창을 마우스로 끌었을 때 부모 창이 따라오지 않는지 확인 (X11 명령으로 옮겨 보는 확인은 마침)

## 추가로 고려할 것

- 글자가 클 때 그리드 칸을 넓혀서 파일 이름이 꺾이지 않게 하기
- 나머지 모달 대화상자도 비모달로 바꾸기
- 저장된 위치에 창 크기를 더하면 화면을 벗어날 때, 창을 화면 안으로 당겨 넣기
