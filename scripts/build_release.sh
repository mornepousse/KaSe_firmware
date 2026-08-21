#!/usr/bin/env bash
# Construit les binaires de release des 3 claviers et les rassemble dans release/.
#
# Usage : ./scripts/build_release.sh vX.Y.Z
# Sortie : release/KaSe_<version>_<HW>.bin       (app seule, à flasher en 0x20000)
#          release/KaSe_<version>_<HW>_full.bin  (image complète, à flasher en 0x0)
#
# ── Pourquoi ce script a été réécrit (2026-08-19) ────────────────────────────
# La version précédente lançait `idf.py -DBOARD=<board> build` sans -B ni
# -DSDKCONFIG : les trois boards se construisaient dans le MÊME dossier build/
# avec le sdkconfig racine. Deux conséquences, toutes deux silencieuses :
#
#   1. Fuite de configuration d'un board à l'autre — ce que CLAUDE.md interdit
#      explicitement.
#   2. Les `sdkconfig.defaults.<short>` par board n'étaient JAMAIS lus, parce
#      qu'ils ne le sont qu'à la génération d'un sdkconfig neuf et qu'un
#      sdkconfig racine existait déjà. Le V2D serait sorti avec le BLE compilé
#      malgré sdkconfig.defaults.v2_debug.
#
# D'où : un dossier de build ET un sdkconfig par board, comme le reste du dépôt.
set -euo pipefail

if [ -z "${IDF_PATH:-}" ]; then
    source "$HOME/esp/esp-idf/export.sh" 2>/dev/null
fi
export IDF_CCACHE_ENABLE=1

VERSION_TAG="${1:-$(git describe --tags --always)}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$PROJECT_DIR/release"

BOARDS=("kase_v1" "kase_v2" "kase_v2_debug")
HW_NAMES=("V1" "V2" "V2_Debug")

mkdir -p "$RELEASE_DIR"
cd "$PROJECT_DIR"

for i in "${!BOARDS[@]}"; do
    board="${BOARDS[$i]}"
    hw="${HW_NAMES[$i]}"
    bdir="build_${board}"

    echo ""
    echo "======================================== $board"
    idf.py -B "$bdir" -DBOARD="$board" -DSDKCONFIG="$bdir/sdkconfig" build 2>&1 | tail -3

    cp "$bdir/KeSp.bin" "$RELEASE_DIR/KaSe_${VERSION_TAG}_${hw}.bin"

    # Image complète : bootloader + table de partitions + otadata + app + storage.
    # Se flashe en 0x0 après un erase_flash ; requise après tout changement de
    # table de partitions.
    ( cd "$bdir" && esptool.py --chip esp32s3 merge_bin \
        -o "$RELEASE_DIR/KaSe_${VERSION_TAG}_${hw}_full.bin" \
        --flash_mode dio --flash_freq 80m --flash_size 16MB \
        0x0 bootloader/bootloader.bin \
        0x8000 partition_table/partition-table.bin \
        0x19000 ota_data_initial.bin \
        0x20000 KeSp.bin \
        0x420000 storage.bin > /dev/null )

    echo "  -> release/KaSe_${VERSION_TAG}_${hw}.bin (+ _full)"
done

echo ""
echo "======================================== artefacts"
ls -lh "$RELEASE_DIR"/KaSe_"$VERSION_TAG"_*.bin | awk '{print "  "$5"\t"$9}'

# Garde-fou : le V2D ne doit pas embarquer le BLE (sdkconfig.defaults.v2_debug).
# Si ce contrôle échoue, c'est que les defaults par board n'ont pas été pris —
# exactement la panne silencieuse que la réécriture de ce script corrige.
if grep -q "^CONFIG_BT_ENABLED=y" build_kase_v2_debug/sdkconfig 2>/dev/null; then
    echo ""
    echo "ERREUR : le V2D embarque le BLE — sdkconfig.defaults.v2_debug n'a pas été pris." >&2
    echo "         Supprimer build_kase_v2_debug/sdkconfig et relancer." >&2
    exit 1
fi
echo "  ✓ V2D sans BLE (defaults par board bien pris en compte)"
