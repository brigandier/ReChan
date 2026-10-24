import importlib.util
from pathlib import Path
import unittest
from unittest.mock import Mock

spec = importlib.util.spec_from_file_location('localization', Path(__file__).resolve().parents[1] / 'scripts/validate-localization.py')
localization = importlib.util.module_from_spec(spec)
spec.loader.exec_module(localization)


class CatalogIntegrity(unittest.TestCase):
    def test_order_reports_first_difference(self):
        with self.assertRaisesRegex(AssertionError, "position', 2.*expected', 'B'.*got', 'C'"):
            localization.validate_order(dict.fromkeys(['A', 'B', 'C']), dict.fromkeys(['A', 'C', 'B']), 'fixture')

    def test_missing_and_extra(self):
        for keys in (['A'], ['A', 'B', 'C']):
            with self.assertRaisesRegex(AssertionError, 'token mismatch'):
                localization.validate_order(dict.fromkeys(['A', 'B']), dict.fromkeys(keys), 'fixture')

    def test_duplicate_is_rejected(self):
        path = Mock()
        path.read_text.return_value = '[A]\nOne\n[A]\nTwo\n'
        with self.assertRaisesRegex(AssertionError, 'duplicate'):
            localization.catalog(path)

    def test_canonical_categories(self):
        root = Path(__file__).resolve().parents[1] / 'res/pc/text'
        reference = localization.catalog(root / 'english.txt')
        keys = list(reference)
        for preceding, following in [('FE_LSPA', 'FE_LPOR'), ('FE_NX', 'FE_PS'), ('FE_VCV', 'FE_SUB')]:
            self.assertEqual(keys[keys.index(preceding) + 1], following)
        for path in root.glob('*.txt'):
            localization.validate_order(reference, localization.catalog(path), path)
            contents = path.read_text(encoding='utf-8-sig')
            self.assertLess(contents.index('# PSX overrides'), contents.index('[0xC131692D]'))


if __name__ == '__main__':
    unittest.main()
