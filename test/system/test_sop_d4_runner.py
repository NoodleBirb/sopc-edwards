"""T49: guard against false positives in the D4 test runner."""

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

import sop_d4
import run as system_runner


class RuntimeOracleTest(unittest.TestCase):
    def setUp(self):
        self.case = {"id": "T8", "kind": "runtime", "source": "runtime/example.top",
                     "category": "OutOfBounds", "before": [111]}

    def process(self, status=255, out="Program output: 111\n", error="OutOfBounds\n"):
        return subprocess.CompletedProcess([], status, out, error)

    def test_expected_runtime_failure_passes(self):
        self.assertEqual(sop_d4.check_execution(self.case, self.process()), "")

    def test_zero_status_is_not_a_runtime_failure(self):
        self.assertTrue(sop_d4.check_execution(self.case, self.process(status=0)))

    def test_signal_is_not_the_intended_error(self):
        self.assertTrue(sop_d4.check_execution(self.case, self.process(status=-11)))

    def test_wrong_or_missing_category_fails(self):
        for diagnostic in ["", "ZeroStep", "NotOutOfBounds"]:
            with self.subTest(diagnostic=diagnostic):
                self.assertTrue(sop_d4.check_execution(self.case, self.process(error=diagnostic)))

    def test_post_failure_output_fails(self):
        process = self.process(out="Program output: 111\nProgram output: 999\n")
        self.assertTrue(sop_d4.check_execution(self.case, process))

    def test_missing_or_reordered_effects_fail(self):
        self.case["before"] = [111, 1, 2, 3]
        for trace in [[111, 1, 3, 2], [111, 1, 2], [111, 1, 2, 2, 3]]:
            out = "".join(f"Program output: {value}\n" for value in trace)
            with self.subTest(trace=trace):
                self.assertTrue(sop_d4.check_execution(self.case, self.process(out=out)))

    def test_sanitizer_report_fails_even_with_zero_status(self):
        valid = {"kind": "functional"}
        for report in ["ERROR: AddressSanitizer: heap-use-after-free", "ERROR: LeakSanitizer"]:
            with self.subTest(report=report):
                process = self.process(0, "Program output: 0\n", report)
                self.assertTrue(sop_d4.check_execution(valid, process))

    def test_success_requires_expected_output(self):
        valid = {"kind": "functional"}
        self.assertEqual(sop_d4.check_execution(valid, self.process(0, "Program output: 0\n", "")), "")
        self.assertTrue(sop_d4.check_execution(valid, self.process(0, "", "")))

    def test_compile_failure_cannot_satisfy_runtime_oracle(self):
        settings = sop_d4.Settings(Path("fake-topc"), "fake-clang", Path("runtime.bc"))
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(sop_d4, "run_command", return_value=self.process(status=1)) as run:
                results = sop_d4.run_case(self.case, "optimized", settings, Path(directory))
        self.assertFalse(results[0].passed)
        self.assertIn("compile failed", results[0].message)
        self.assertEqual(run.call_count, 1)

    def test_link_failure_cannot_satisfy_runtime_oracle(self):
        settings = sop_d4.Settings(Path("fake-topc"), "fake-clang", Path("runtime.bc"))
        success = self.process(0, "", "")
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(sop_d4, "run_command", side_effect=[success, self.process(status=1)]) as run:
                results = sop_d4.run_case(self.case, "optimized", settings, Path(directory))
        self.assertFalse(results[0].passed)
        self.assertIn("link failed", results[0].message)
        self.assertEqual(run.call_count, 2)

    def test_junit_accepts_colored_compiler_diagnostics(self):
        result = sop_d4.Result("T8.optimized", False,
                               "\x1b[31mcompile failed: <bad>\x1b[0m\x00")
        for writer in [sop_d4.write_junit, system_runner.write_junit]:
            with self.subTest(writer=writer.__module__):
                with tempfile.TemporaryDirectory() as directory:
                    destination = Path(directory) / "report.xml"
                    writer([result], destination)
                    report = ET.parse(destination).getroot()
                self.assertEqual(report.find("testcase/failure").text,
                                 "compile failed: <bad>")


if __name__ == "__main__":
    unittest.main()
