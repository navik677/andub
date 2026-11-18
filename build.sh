#!/usr/bin/env bash
set -e

echo -e "\033[1;36m==> Збірка Andub (Qt 6 Quick + C++20)...\033[0m"

if ! command -v cmake &>/dev/null; then
    echo -e "\033[1;33m[Попередження]\033[0m cmake не знайдено. Запустіть ./install.sh, щоб встановити залежності."
    exit 1
fi

GENERATOR=()
command -v ninja &>/dev/null && GENERATOR=(-G Ninja)

cmake -S . -B build "${GENERATOR[@]}" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

echo ""
echo -e "\033[1;32m[✓] Збірка успішно завершена!\033[0m Запустити додаток:"
echo -e "    \033[1;36m./build/andub\033[0m"
