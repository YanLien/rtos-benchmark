#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <elf>" >&2
    exit 2
fi

ELF="$1"

if readelf -r "$ELF" | grep -q '\.rela\.dyn'; then
    echo "FAIL: $ELF still contains a .rela.dyn section, so bare-metal function/data pointers depend on runtime relocation." >&2
    exit 1
fi

echo "PASS: $ELF has no dynamic relocation section"
