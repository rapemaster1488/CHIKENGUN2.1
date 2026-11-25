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
