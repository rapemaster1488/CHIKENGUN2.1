#!/usr/bin/env node
// ============================================================================
// CHIKENGUN — Asset integrity check (ловит класс «забыли положить файл» → 404 в проде).
//
// Что делает:
//   1. Сканирует *.html + web/*.js и ищет ВСЕ строковые литералы с путями
//      ассетов: "Data/...", "CoreData/..." (существование обязательно),
//      а также голые "name.wav/.ogg/.png" (ищем по типовым каталогам движка).
//   2. Для TexturesHD-путей отсутствие НЕ фатал (в движке есть фолбэк на 512px SD)
//      — предупреждение; для SD/Sounds/Music — падение с ненулевым кодом.
//
// Запуск:  node tools/asset_check.js
// ============================================================================
"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.join(__dirname, "..");
const SCAN = process.argv.slice(2);
const targets = SCAN.length ? SCAN : [
  "index.html", "banya.html", "casino.html", "minigames.html",
  "web/webgl2_arena.js"
].map(f => path.join(ROOT, f)).filter(f => fs.existsSync(f));

const SEARCH_DIRS = ["Data/Sounds", "Data/Data/Sounds", "Data/Music", "Data/Textures", "Data/TexturesHD"];
const errors = [], warnings = [], checked = new Set();

const exists = rel => fs.existsSync(path.join(ROOT, rel));

function checkToken(tok, fromFile) {
  if (checked.has(tok)) return;
  checked.add(tok);
  if (tok.includes("${") || tok.includes("*") || tok.includes("..")) return;   // шаблоны/маски — пропускаем
  const base = path.basename(tok);
  const looksHD = tok.includes("TexturesHD");

  if (tok.includes("/")) {                                    // путь с каталогом
    if (!/^(Data|CoreData|bin)\//.test(tok)) return;          // чужие пути (url, src) не трогаем
    if (exists(tok)) return;
    if (looksHD) { warnings.push(`[HD 404-tolerated] ${tok}  (referenced in ${fromFile}; движок откатится на SD)`); return; }
    errors.push(`[MISSING] ${tok}  (referenced in ${fromFile})`);
    return;
  }
  if (/\.(wav|ogg)$/i.test(tok)) {                            // голое имя звука
    const hit = SEARCH_DIRS.find(d => exists(`${d}/${tok}`));
    if (!hit) errors.push(`[MISSING] ${tok}  (не найден ни в ${SEARCH_DIRS.join(", ")}; referenced in ${fromFile})`);
    return;
  }
  if (/\.png$/i.test(tok)) {                                  // голое имя текстуры
    const hit = ["Data/Textures", "Data/TexturesHD"].find(d => exists(`${d}/${tok}`));
    if (!hit) warnings.push(`[PNG?] ${tok}  (не найден в Data/Textures[HD]; referenced in ${fromFile})`);
  }
}

const RE = /"([^"\n]{3,120})"/g;   // строковые литералы в кавычках
for (const file of targets) {
  const rel = path.relative(ROOT, file);
  const src = fs.readFileSync(file, "utf8");
  let m;
  while ((m = RE.exec(src))) {
    const tok = m[1].trim();
    if (/\.(wav|ogg|png|jpg|jpeg|webp|bmp)$/i.test(tok)) checkToken(tok, rel);
  }
}

console.log(`[asset-check] просканировано файлов: ${targets.length}, уникальных ассетов: ${checked.size}`);
for (const w of warnings) console.log("WARN " + w);
if (errors.length) {
  for (const e of errors) console.error("FAIL " + e);
  console.error(`[asset-check] ПРОВАЛ: ${errors.length} ассет(ов) не найдено`);
  process.exit(1);
}
console.log(`[asset-check] OK — все обязательные ассеты на месте (${warnings.length} предупреждений)`);
