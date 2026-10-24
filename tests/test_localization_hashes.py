import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('localization', Path(__file__).resolve().parents[1] / 'scripts/validate-localization.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class LocalizationHashes(unittest.TestCase):
    def test_runtime_hash_vectors(self):
        self.assertEqual(validator.token_hash('FE_SUB'), 0xa85f3b14)
        self.assertEqual(validator.token_hash('0xC131692D'), 0xc131692d)
        self.assertEqual(validator.token_hash('3241249069'), 3241249069)
        self.assertEqual(validator.token_hash('010'), 8)
        self.assertEqual(validator.token_hash('\u0661'), 0xd9 * 33 + 0xa1)

    def test_named_collision(self):
        with self.assertRaisesRegex(AssertionError, 'xcHash collision'):
            validator.validate_hashes({'Aa': 'one', 'B@': 'two'}, 'synthetic.txt')

    def test_numeric_alias_collision(self):
        alias = f'0x{validator.token_hash("FE_SUB"):08X}'
        with self.assertRaisesRegex(AssertionError, 'FE_SUB'):
            validator.validate_hashes({'FE_SUB': 'one', alias: 'two'}, 'synthetic.txt')

    def test_distinct_tokens(self):
        validator.validate_hashes({'FE_SUB': 'one', 'FE_LPOR': 'two', '0xC131692D': 'three'}, 'synthetic.txt')


if __name__ == '__main__':
    unittest.main()
