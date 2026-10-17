#!/bin/sh
# Smoke test do cosmofetch: roda cada modulo e cada logo isoladamente,
# valida o JSON e confere os codigos de saida.
# Uso: sh tests/smoke.sh ./cosmofetch
BIN="${1:-./cosmofetch}"
fail=0
ok()   { printf '  ok    %s\n' "$1"; }
bad()  { printf '  FALHA %s\n' "$1"; fail=$((fail + 1)); }

echo "== opcoes basicas =="
"$BIN" --version >/dev/null && ok "--version" || bad "--version"
"$BIN" --help >/dev/null && ok "--help" || bad "--help"
"$BIN" --list-modules >/dev/null && ok "--list-modules" || bad "--list-modules"
"$BIN" --list-logos >/dev/null && ok "--list-logos" || bad "--list-logos"
"$BIN" --gen-config >/dev/null && ok "--gen-config" || bad "--gen-config"
"$BIN" --nao-existe >/dev/null 2>&1 && bad "opcao invalida deveria falhar" || ok "opcao invalida falha"

echo "== cada modulo isolado =="
for m in $("$BIN" --list-modules | awk 'NF>=2 && $1 !~ /^\[/ && $1 !~ /^[0-9]/ {print $1}'); do
    if timeout 20 "$BIN" --no-config --no-color --no-logo --structure "$m" >/dev/null 2>&1; then
        ok "$m"
    else
        bad "$m"
    fi
done

echo "== cada logo =="
for l in $("$BIN" --list-logos | awk '{print $1}'); do
    if "$BIN" --no-config --no-color --logo "$l" --structure title:os >/dev/null 2>&1; then
        ok "logo $l"
    else
        bad "logo $l"
    fi
done

echo "== layouts e temas =="
for pos in left right top none; do
    "$BIN" --no-config --color --logo-position "$pos" --structure title:separator:os:cpu:colors >/dev/null 2>&1 \
        && ok "logo-position $pos" || bad "logo-position $pos"
done
for t in cosmo ocean sunset forest mono neon; do
    "$BIN" --no-config --color --theme "$t" --structure title:os >/dev/null 2>&1 && ok "theme $t" || bad "theme $t"
done
"$BIN" --no-config --bars --structure memory:disk:cpuusage >/dev/null 2>&1 && ok "--bars" || bad "--bars"

echo "== JSON =="
if command -v python3 >/dev/null 2>&1; then
    if "$BIN" --no-config --json | python3 -c 'import json,sys; d=json.load(sys.stdin); assert isinstance(d,list) and len(d)>0'; then
        ok "JSON valido"
    else
        bad "JSON invalido"
    fi
else
    echo "  (python3 ausente, pulando validacao do JSON)"
fi

echo "== config customizada =="
tmp="$(mktemp)"
printf 'theme = ocean\ncustom1 = Eco | echo ola\nstructure = title:custom1\n' > "$tmp"
if "$BIN" --config "$tmp" --no-color --no-logo | grep -q 'Eco: ola'; then ok "custom1 via config"; else bad "custom1 via config"; fi
rm -f "$tmp"

echo
if [ "$fail" -eq 0 ]; then echo "Tudo certo."; else echo "$fail falha(s)."; fi
exit "$fail"
