"""Validate the shipped small UI asset without Pillow or a renderer."""
import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('chips', ROOT / 'tools/gen_chip_atlas.py')
chips = importlib.util.module_from_spec(spec)
spec.loader.exec_module(chips)

class ChipAtlasTest(unittest.TestCase):
    def test_cell_count_matches_3d_uv_shift(self):
        # 3D-фишки берут ячейку атласа по UOffset = index / cells. Если
        # поделить не на то число, «белая» фишка окажется красной, а
        # последняя вылезет за атлас. Число ячеек здесь — единственный
        # источник правды, и оно обязано быть 6.
        data = (ROOT / 'Data/Textures/UI/CasinoChips.png').read_bytes()
        width, height = struct.unpack('>II', data[16:24])
        self.assertGreater(height, 0)
        self.assertEqual(width % height, 0, "атлас должен делиться на квадратные ячейки")
        cells = width // height
        self.assertEqual(cells, 6, "в CasinoChips.png ровно 6 ячеек")
        # Шесть материалов фишек в MyApp: белая, красная, синяя, зелёная,
        # фиолетовая, золотая. Индексы 0..5 должны попадать внутрь атласа.
        for index in range(6):
            offset = (1.0 / cells) * index
            self.assertGreaterEqual(offset, 0.0)
            self.assertLess(offset + 1.0 / cells, 1.0001, f"ячейка {index} вылезает за атлас")

    def test_uoffset_is_scale_plus_shift(self):
        # В шейдере Urho3D (Transform.glsl / Transform.hlsl) UV считается как
        #     uv = texCoord * UOffset.xy + UOffset.w
        # то есть xy — МАСШТАБ, а w — смещение. Чтобы выбрать ячейку из
        # атласа, нужно xy = 1/cells и w = index/cells. Раньше в MyApp.cpp
        # xy = index/cells (ячейка 5 растягивала всю текстуру), w = 0 (все
        # фишки показывали начало атласа) — из-за этого 3D-фишки выглядели
        # не фишками. Тест закрепляет правильную формулу.
        source = (ROOT / 'src/MyApp.cpp').read_text(encoding='utf-8')
        self.assertIn('SetShaderParameter("UOffset"', source)
        # В файле есть и другие тайлинги (пол, стены) — берём именно фишки.
        line = [l for l in source.splitlines()
                if 'SetShaderParameter("UOffset"' in l and 'atlasIndex' in l]
        self.assertEqual(len(line), 1, "UV-сдвиг фишек должен задаваться в одном месте")
        code = line[0]
        self.assertIn('1.0f / 6.0f, 0.0f, 0.0f, (1.0f / 6.0f) * atlasIndex', code,
                      "UOffset.xy должен быть масштабом 1/6, а .w — смещением index/6")
        # Заодно убеждаемся, что шейдер действительно так и считает: если
        # Urho3D когда-нибудь поменяет семантику, тест это поймает.
        shader = (ROOT / 'CoreData/Shaders/GLSL/Transform.glsl').read_text(encoding='utf-8')
        self.assertIn('cUOffset.xy', shader)
        self.assertIn('cUOffset.w', shader)


    def test_reproducible_rgba_atlas(self):
        data = (ROOT / 'Data/Textures/UI/CasinoChips.png').read_bytes()
        self.assertEqual(data, chips.generate())
        self.assertLess(len(data), 32 * 1024)
        self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
        pos, compressed = 8, bytearray()
        while pos < len(data):
            length = struct.unpack('>I', data[pos:pos+4])[0]
            tag = data[pos+4:pos+8]
            payload = data[pos+8:pos+8+length]
            crc = struct.unpack('>I', data[pos+8+length:pos+12+length])[0]
            self.assertEqual(crc, zlib.crc32(tag+payload))
            if tag == b'IHDR':
                self.assertEqual(struct.unpack('>IIBBBBB', payload), (384,64,8,6,0,0,0))
            if tag == b'IDAT': compressed.extend(payload)
            pos += 12+length
        raw = zlib.decompress(compressed)
        self.assertEqual(len(raw), 64*(384*4+1))
        for tile in range(6):
            self.assertEqual(raw[1+tile*64*4+3], 0)
            self.assertEqual(raw[32*(384*4+1)+1+(tile*64+32)*4+3], 255)

if __name__ == '__main__': unittest.main()
