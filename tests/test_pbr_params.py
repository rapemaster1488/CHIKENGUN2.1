"""Страж против повторения регрессии «визуал откатился до v1».

ЧТО СЛУЧИЛОСЬ (PR #44). Я выставил материалам, которые наследуют PBR-карту
(CasinoFelt / CasinoGold / CasinoMahogany), абсолютные Roughness и Metallic:

    brassMat->SetShaderParameter("Metallic", 0.82f);
    feltInlayMat->SetShaderParameter("Roughness", 0.98f);

Но в шейдере Urho3D они ПРИБАВЛЯЮТСЯ к карте, а не заменяют её:

    float roughness = roughMetalSrc.r + cRoughness;
    float metalness = roughMetalSrc.g + cMetallic;
    roughness *= roughness;
    ...
    diffColor.rgb = diffColor.rgb - diffColor.rgb * metalness;   // metal=1 → чёрный

Металл уходил в 1.0, диффуз обнулялся — борт стола и ободки фишек
становились чёрными, сукно теряло блик. Это и был «откат до v1».

Этот тест читает исходник и запрещает задавать Roughness/Metallic
материалам, у которых есть карта. Правильный способ — двигать значение
ВЫЧИТАНИЕМ (как давно сделано для скинов: Metallic -0.90f) либо не трогать
эти параметры вообще, а менять только цвет и MatSpecColor.
"""
import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "MyApp.cpp"

# Материалы, у которых есть PBR-карта: их Roughness/Metallic уже заняты.
MAP_MATERIALS = ("feltInlayMat", "brassMat", "carpetMat", "chipRimMat",
                 "wheelGoldMat", "wheelGoldDimMat", "wheelLacquerMat")


class PbrParamGuardTest(unittest.TestCase):
    def test_source_readable(self):
        self.assertTrue(SRC.is_file())
        text = SRC.read_text(encoding="utf-8")
        # Шейдер, вокруг которого вся логика, должен быть на месте.
        self.assertIn("roughMetalSrc.r + cRoughness", self._shader())

    def _shader(self):
        """Текст PBR-шейдера из локальной копии движка (если доступна)."""
        candidates = [
            ROOT / "CoreData" / "Shaders" / "GLSL" / "PBRLitSolid.glsl",
        ]
        for c in candidates:
            if c.is_file():
                return c.read_text(encoding="utf-8", errors="ignore")
        self.skipTest("PBR-шейдер не поставляется с репозиторием")

    def test_no_absolute_roughness_or_metallic_on_mapped_materials(self):
        text = SRC.read_text(encoding="utf-8")
        offenders = []
        for mat in MAP_MATERIALS:
            # Ловим именно вызовы вида <mat>->SetShaderParameter("Metallic", 0.82f)
            for param in ("Roughness", "Metallic"):
                pattern = re.compile(
                    rf'{re.escape(mat)}\s*->\s*SetShaderParameter\(\s*"{param}"\s*,\s*'
                    r'([-+]?[0-9]*\.?[0-9]+)f?\s*\)')
                for m in pattern.finditer(text):
                    value = float(m.group(1))
                    # Допустимо только ВЫЧИТАНИЕ: отрицательное смещение к карте.
                    # Ноль тоже безопасен (ничего не меняет), но и он не нужен.
                    if value >= 0.0:
                        offenders.append(f"{mat}.{param} = {value} (прибавится к карте)")
        self.assertEqual(offenders, [],
                         "Не задавай абсолютные Roughness/Metallic материалам с "
                         "PBR-картой — они ПРИБАВЛЯЮТСЯ к карте и обнуляют диффуз. "
                         "Только отрицательное смещение или MatDiffColor. "
                         "Нарушения: " + "; ".join(offenders))

    def test_mapped_materials_keep_their_texture(self):
        """Если материал клонируется из HD-текстуры, клон обязан сохранять её."""
        text = SRC.read_text(encoding="utf-8")
        # Клоны должны делаться от hd*_ ресурса, а не от MakePBR, иначе
        # текстура теряется и материал снова становится «пластиковым шаром».
        for mat in ("feltInlayMat", "brassMat", "chipRimMat"):
            decl = re.search(rf'{mat}\s*=\s*hdCasino\w+_\s*\?', text)
            self.assertIsNotNone(decl, f"{mat} должен клонироваться из HD-материала")

    def test_felt_has_both_tiling_axes(self):
        """UOffset без VOffset = текстура схлопывается по вертикали.

        Именно это я сделал в PR #44: задал UOffset (масштаб по U) и забыл
        VOffset, поэтому сукно по V растянулось в одну строку пикселей.
        """
        text = SRC.read_text(encoding="utf-8")
        block = text[text.find("feltInlayMat"):]
        block = block[:block.find("brassMat")]
        self.assertIn('"UOffset"', block, "у сукна должен быть тайлинг по U")
        self.assertIn('"VOffset"', block, "у сукна должен быть тайлинг по V")


if __name__ == "__main__":
    unittest.main(verbosity=2)
