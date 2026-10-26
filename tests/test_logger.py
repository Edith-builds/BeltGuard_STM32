import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('logger', Path(__file__).parents[1]/'host/logger.py')
logger = importlib.util.module_from_spec(spec)
spec.loader.exec_module(logger)
class ProtocolTest(unittest.TestCase):
    def test_crc_vector(self):
        self.assertEqual(logger.crc16(b'123456789'), 0x4b37)
    def test_decode(self):
        p=b'{"v":1,"node":1,"health_index":null}'
        self.assertEqual(logger.decode(p+b'*'+f'{logger.crc16(p):04X}'.encode()+b'\n')['node'],1)
    def test_corruption(self):
        with self.assertRaises(ValueError): logger.decode(b'{"v":1,"node":1}*0000\n')
    def test_bad_frame(self):
        with self.assertRaises(ValueError): logger.decode(b'noise')
