import unittest

from math_normalize import normalize_math_powers


class MathNormalizeTests(unittest.TestCase):
    def test_caret_power_becomes_latex(self):
        self.assertIn("$x^{2}$", normalize_math_powers("Die Ableitung von x^2"))

    def test_digit_juxtaposition_becomes_latex(self):
        out = normalize_math_powers("Betrachte x2 und y3.")
        self.assertIn("$x^{2}$", out)
        self.assertIn("$y^{3}$", out)

    def test_existing_math_untouched(self):
        src = "Schon gut: $a^{n}$ und $$E=mc^{2}$$"
        self.assertEqual(normalize_math_powers(src), src)

    def test_code_fence_untouched(self):
        src = "Text\n```\nx^2\n```\nok"
        self.assertIn("```\nx^2\n```", normalize_math_powers(src))


if __name__ == "__main__":
    unittest.main()
