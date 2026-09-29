#!/bin/bash
# ApexOS -- Génère le miroir Includes/ à partir de tous les .c de ApexKernel/
# (hors ApexKernel/Includes/ lui-même). Un .h vide par .c, même chemin relatif,
# juste l'extension qui change.
#
# Usage : à lancer depuis la racine ApexProject/ (le dossier qui contient ApexKernel/)
#   bash mirror_includes.sh

set -e

SRC_ROOT="ApexKernel"
DST_ROOT="ApexKernel/Includes"

if [ ! -d "$SRC_ROOT" ]; then
    echo "Erreur : dossier $SRC_ROOT introuvable. Lance ce script depuis la racine ApexProject/."
    exit 1
fi

count=0

# Trouve tous les .c sous ApexKernel/, en excluant ApexKernel/Includes/
find "$SRC_ROOT" -path "$DST_ROOT" -prune -o -type f -name "*.c" -print | while read -r cfile; do
    # Chemin relatif à ApexKernel/ (ex: Core/Memory/Heap/Heap.c)
    rel_path="${cfile#$SRC_ROOT/}"

    # Même chemin mais en .h, sous Includes/
    h_path="$DST_ROOT/${rel_path%.c}.h"
    h_dir=$(dirname "$h_path")

    mkdir -p "$h_dir"

    if [ -f "$h_path" ]; then
        echo "SKIP (existe déjà) : $h_path"
    else
        touch "$h_path"
        echo "CREATED : $h_path"
    fi

    count=$((count + 1))
done

echo ""
echo "Terminé."
