"""I/O contract tests. Do not mistake the tests for CVNSS4.0 rule tests."""
import unittest

from tools.cvnss_audit import audit_rows


class CVNSSAuditTests(unittest.TestCase):
    def test_roundtrip_with_valid_ascii_payload(self):
        # Any ASCII payload can pass transport; this is NOT a rule-conversion test.
        rows = [{"unicode_source": "Tiếng Việt", "cvnss_ascii": "TEST",
                 "unicode_restored": "Tiếng Việt"}]
        report = audit_rows(rows)
        self.assertTrue(report["pass"])
        self.assertEqual(report["unicode_nfc_exact_matches"], 1)

    def test_reject_unsupported_cvns_symbol(self):
        rows = [{"unicode_source": "xin chào", "cvnss_ascii": "xin chào",
                 "unicode_restored": "xin chào"}]
        report = audit_rows(rows)
        self.assertFalse(report["pass"])
        self.assertEqual(report["valid_base27_roundtrips"], 0)

    def test_reject_mismatched_unicode(self):
        rows = [{"unicode_source": "má", "cvnss_ascii": "MA",
                 "unicode_restored": "mà"}]
        report = audit_rows(rows)
        self.assertFalse(report["pass"])
        self.assertEqual(report["unicode_nfc_exact_matches"], 0)

    def test_allow_decomposed_unicode_equivalent(self):
        rows = [{"unicode_source": "á", "cvnss_ascii": "A",
                 "unicode_restored": "a\u0301"}]
        self.assertTrue(audit_rows(rows)["pass"])

    def test_empty_dataset_does_not_pass(self):
        self.assertFalse(audit_rows([])["pass"])

    def test_missing_field(self):
        rows = [{"unicode_source": "abc", "cvnss_ascii": "ABC"}]
        self.assertFalse(audit_rows(rows)["pass"])


if __name__ == "__main__":
    unittest.main()
