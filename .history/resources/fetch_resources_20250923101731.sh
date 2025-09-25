#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
MANIFEST="$ROOT_DIR/manifest.json"
DOWNLOAD_DIR="$ROOT_DIR"

# Colors
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'

need_tool() { command -v "$1" >/dev/null 2>&1 || { echo -e "${RED}Required tool '$1' not found${NC}"; exit 1; }; }
need_tool curl
need_tool jq

fetch_file() {
  local name=$1 url=$2 target=$3
  local dest="$DOWNLOAD_DIR/$target/$name"
  if [[ -f "$dest" ]]; then
    echo -e "${YELLOW}[SKIP]${NC} $name already exists"
    return 0
  fi
  if [[ "$url" == *"<"*">"* ]]; then
    echo -e "${YELLOW}[MANUAL]${NC} Placeholder for $name (URL requires manual retrieval: $url)"
    return 0
  fi
  echo -e "${GREEN}[GET]${NC} $name"
  if ! curl -L --retry 3 --fail -o "$dest" "$url"; then
    echo -e "${RED}[FAIL]${NC} $name from $url"
    return 1
  fi
}

process_section() {
  local section=$1
  jq -c ".$section[]" "$MANIFEST" 2>/dev/null | while read -r line; do
    name=$(echo "$line" | jq -r .name)
    url=$(echo "$line" | jq -r .url)
    target=$(echo "$line" | jq -r .target_dir)
    mkdir -p "$DOWNLOAD_DIR/$target"
    fetch_file "$name" "$url" "$target" || true
  done
}

echo "== High Priority =="; process_section high_priority
echo "== Medium Priority =="; process_section medium_priority
echo "== Optional =="; process_section optional

echo -e "${GREEN}Done.${NC} Review MANUAL items for manual download."
