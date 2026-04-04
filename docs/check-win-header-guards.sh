#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="${1:-.}"
FAIL_ON_VIOLATION="${2:-}"

cd "$REPO_ROOT"

if [[ ! -d src ]]; then
  echo "Cannot find src directory under: $REPO_ROOT" >&2
  exit 2
fi

# Windows-only implementation units are allowed to include Windows headers directly.
is_windows_unit() {
  local rel="$1"
  [[ "$rel" == *"/win/"* ]] && return 0
  [[ "$rel" == *"_win.cpp" ]] && return 0
  [[ "$rel" == *"_win.h" ]] && return 0
  [[ "$rel" == *"/win_"*".cpp" ]] && return 0
  [[ "$rel" == *"/win_"*".h" ]] && return 0
  [[ "$rel" == src/video/capturer/dxgi/* ]] && return 0
  [[ "$rel" == src/video2/* ]] && return 0
  [[ "$rel" == src/app/select_gpu.cpp ]] && return 0
  [[ "$rel" == src/inputs/executor/gamepad.cpp ]] && return 0
  [[ "$rel" == src/inputs/executor/input_executor.cpp ]] && return 0
  [[ "$rel" == src/service/workers/worker_process.cpp ]] && return 0
  [[ "$rel" == src/video/cepipeline/video_capture_encode_pipeline.cpp ]] && return 0
  [[ "$rel" == src/worker/display_setting.cpp ]] && return 0
  [[ "$rel" == src/worker/session_change_observer.cpp ]] && return 0
  return 1
}

violations=()

while IFS= read -r -d '' file; do
  rel="${file#./}"

  if is_windows_unit "$rel"; then
    continue
  fi

  while IFS=: read -r line_no line_text; do
    [[ -z "$line_no" ]] && continue

    start=$((line_no > 20 ? line_no - 20 : 1))
    context=$(sed -n "${start},${line_no}p" "$file")

    if grep -Eq '^\s*#\s*(if|ifdef)\b.*(LT_WINDOWS|_WIN32|WIN32|defined\s*\(\s*LT_WINDOWS\s*\)|defined\s*\(\s*_WIN32\s*\))' <<<"$context"; then
      continue
    fi

    violations+=("${rel}:${line_no}: ${line_text}")
  done < <(grep -nE '^\s*#\s*include\s*(<|\")((Windows\.h|windows\.h)|(ltlib/win_service\.h))(>|\")' "$file" || true)
done < <(find ./src -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.m' -o -name '*.mm' \) -print0)

if [[ ${#violations[@]} -eq 0 ]]; then
  echo "No unguarded Windows.h/win_service.h includes found in non-Windows units."
  exit 0
fi

echo "Detected unguarded Windows include(s) in non-Windows units:"
for v in "${violations[@]}"; do
  echo "$v"
done

if [[ "$FAIL_ON_VIOLATION" == "--fail" ]]; then
  exit 1
fi

exit 0
