#!/usr/bin/env bash
set -euo pipefail

# Sync script: copies tflm_esp32 tree to DATN_Docs and commits+pushes to remote repo.
# Usage: run once or as a watcher. Requires: git, rsync, inotifywait (optional for watch mode).

SRC_DIR="/home/namng/Arduino/libraries/tflm_esp32"
TARGET_DIR="/home/namng/DATN_Docs/tflm_esp32_lib"
REMOTE_REPO="git@github.com:nammannam/DATN_tflm_esp32_lib.git"

timestamp() { date -u +"%Y-%m-%dT%H:%M:%SZ"; }

ensure_target_repo() {
  # Ensure target directory exists
  mkdir -p "$TARGET_DIR"

  # If it's not a git repo yet, initialize and set remote
  if [ ! -d "$TARGET_DIR/.git" ]; then
    git -C "$TARGET_DIR" init >/dev/null 2>&1 || true
    git -C "$TARGET_DIR" remote add origin "$REMOTE_REPO" >/dev/null 2>&1 || true
    # create or switch to main branch
    git -C "$TARGET_DIR" checkout -B main >/dev/null 2>&1 || true
    git -C "$TARGET_DIR" commit --allow-empty -m "Initial commit: repo created by sync script" >/dev/null 2>&1 || true
    git -C "$TARGET_DIR" push -u origin main >/dev/null 2>&1 || true
  else
    # ensure remote exists and is set without printing errors
    if ! git -C "$TARGET_DIR" remote get-url origin >/dev/null 2>&1; then
      git -C "$TARGET_DIR" remote add origin "$REMOTE_REPO" >/dev/null 2>&1 || true
    fi
  fi
}

do_sync() {
  echo "[$(timestamp)] Starting sync..."
  # Use rsync to copy all files (preserve permissions), delete removed files in target
  # Exclude the target .git directory when copying into itself (defensive)
  rsync -a --delete --exclude='.git' "$SRC_DIR/" "$TARGET_DIR/"

  cd "$TARGET_DIR"

  # Create a branch name based on UTC timestamp for traceability
  BRANCH_NAME="sync/$(date -u +"%Y-%m-%d-%H%M%S")"

  # Create branch and switch to it
  git checkout -B "$BRANCH_NAME"

  git add -A

  if git diff --cached --quiet; then
    echo "[$(timestamp)] No changes to commit."
  else
    git commit -m "Sync from tflm_esp32 at $(timestamp)"
    # Ensure remote is set
    git remote add origin "$REMOTE_REPO" 2>/dev/null || true
    git push -u origin "$BRANCH_NAME"
    echo "[$(timestamp)] Pushed branch $BRANCH_NAME to origin."
  fi
}

watch_mode() {
  if ! command -v inotifywait >/dev/null 2>&1; then
    echo "inotifywait not found. Install inotify-tools or run script without --watch."
    exit 1
  fi

  echo "Watching $SRC_DIR for changes..."
  while inotifywait -r -e modify,create,delete,move "$SRC_DIR"; do
    do_sync || echo "Sync failed at $(timestamp)"
  done
}

usage() {
  cat <<EOF
Usage: $0 [--once|--watch]
  --once   : perform one sync and exit (default)
  --watch  : watch $SRC_DIR for changes and sync automatically (requires inotifywait)
EOF
}

main() {
  MODE="once"
  if [ "${1-}" = "--watch" ]; then
    MODE="watch"
  elif [ "${1-}" = "--once" ]; then
    MODE="once"
  elif [ "${1-}" = "" ]; then
    MODE="once"
  else
    usage
    exit 2
  fi

  ensure_target_repo

  if [ "$MODE" = "once" ]; then
    do_sync
  else
    watch_mode
  fi
}

main "$@"
