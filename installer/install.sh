#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p "$HOME/.vst3" "$HOME/.lv2" "$HOME/.local/bin" "$HOME/.local/share/guilherme-bolinha"
cp -R 'Guilherme Bolinha.vst3' "$HOME/.vst3/"
cp -R 'Guilherme Bolinha.lv2' "$HOME/.lv2/"
cp 'Guilherme Bolinha' "$HOME/.local/bin/guilherme-bolinha"
cp -R Cure "$HOME/.local/share/guilherme-bolinha/"
printf '%s\n' 'Installed Guilherme Bolinha with its complete sound library.'
