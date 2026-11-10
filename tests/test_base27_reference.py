"""Independently runnable, dependency-free transport reference tests."""
from __future__ import annotations

import random
import unittest

from tools.base27 import ALPHABET, decode, encode
from tools.ternary_budget import calculate


class Base27ReferenceTests(unittest.TestCase):
    def test_alphabet_contains_exactly_27_unique_symbols(self):
        self.assertEqual(len(ALPHABET), 27)
        self.assertEqual(len(set(ALPHABET)), 27)

    def test_all_27_golden_vectors(self):
        for idx, char in enumerate(ALPHABET):
            expected = f"{idx // 9}{(idx // 3) % 3}{idx % 3}"
            with self.subTest(char=repr(char)):
                self.assertEqual(encode(char), expected)
                self.assertEqual(decode(expected), char)

    def test_fixed_examples(self):
        self.assertEqual(encode("ABC"), "000001002")
        self.assertEqual(encode("A Z"), "000222221")
        self.assertEqual(encode(""), "")
        self.assertEqual(decode(""), "")
        self.assertEqual(decode(encode("XIN CHAO VIET NAM")), "XIN CHAO VIET NAM")

    def test_random_roundtrips_are_deterministic(self):
        rng = random.Random(20261110)
        for length in [0, 1, 2, 3, 17, 128, 1000]:
            for _ in range(10):
                payload = "".join(rng.choices(ALPHABET, k=length))
                self.assertEqual(decode(encode(payload)), payload)
                self.assertEqual(len(encode(payload)), 3 * len(payload))

    def test_reject_bad_ascii_and_unicode(self):
        for value in ["a", "Á", "Xin", "A\nB", "A\tB", "A-", "0", "🤖"]:
            with self.subTest(value=repr(value)), self.assertRaises(ValueError):
                encode(value)

    def test_reject_bad_trit_streams(self):
        for value in ["0", "01", "0121", "300", "-10", "00 ", "ABC", "𝟘𝟘𝟘"]:
            with self.subTest(value=repr(value)), self.assertRaises(ValueError):
                decode(value)

    def test_48m_weights_only_budget(self):
        values = calculate(48_000_000)
        self.assertEqual(values["fp32_MB"], 192)
        self.assertEqual(values["int8_MB"], 48)
        self.assertEqual(values["ternary_2bit_MB"], 12)
        self.assertAlmostEqual(values["ternary_entropy_lower_bound_MB"], 9.509775, places=4)

    def test_invalid_parameter_counts(self):
        for n in [0, -1, 0.5]:
            with self.assertRaises(ValueError):
                calculate(n)


if __name__ == "__main__":
    unittest.main()
