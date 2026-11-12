#!/usr/bin/env python3
"""Run D4 programs with separate compile, link, and execution oracles."""

from __future__ import annotations

import argparse
import concurrent.futures
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET


SYSTEM = Path(__file__).resolve().parent
ROOT = SYSTEM.parent.parent
MANIFEST = SYSTEM / "sop-d4-cases.json"
SANITIZER_REPORT = re.compile(r"AddressSanitizer|LeakSanitizer")
OUTPUT_LINE = re.compile(r"^Program output: (-?\d+)$", re.MULTILINE)
ANSI_ESCAPE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
XML_CONTROL = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f]")


def xml_safe_text(message: str) -> str:
    return XML_CONTROL.sub("", ANSI_ESCAPE.sub("", message))


@dataclass
class Result:
    name: str
    passed: bool
    message: str = ""
    duration: float = 0.0


@dataclass
class Settings:
    compiler: Path
    clang: str
    runtime: Path
    sanitize: bool = False
    timeout: int = 30


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text())


def program_cases(suite: str = "all") -> list[dict]:
    cases = [case for case in load_manifest()["cases"] if "source" in case]
    if suite == "runtime":
        return [case for case in cases if case["kind"] == "runtime"]
    if suite == "memory":
        return [case for case in cases if case["kind"] != "runtime"]
    return cases


def check_manifest() -> None:
    manifest = load_manifest()
    cases = manifest["cases"]
    assert [case["id"] for case in cases] == [f"T{i}" for i in range(1, len(cases) + 1)]
    requirements = (ROOT / "REQUIREMENTS4.md").read_text()
    defined = set(re.findall(r"^\*\*(R\d+)\.\*\*", requirements, re.MULTILINE))
    covered = set()
    expected_links = {requirement: set() for requirement in defined}
    for case in cases:
        for number in case["requirements"]:
            assert f"R{number}" in defined, (case["id"], number)
            covered.add(f"R{number}")
            expected_links[f"R{number}"].add(case["id"])
        if "source" in case:
            source = (SYSTEM / case["source"]).resolve()
            assert source.is_relative_to(SYSTEM)
            assert source.is_file(), source
            covered.update(f"R{i}" for i in manifest["common_program_requirements"])
            for number in manifest["common_program_requirements"]:
                expected_links[f"R{number}"].add(case["id"])
            if case["kind"] != "runtime":
                covered.update(f"R{i}" for i in manifest["common_memory_requirements"])
                for number in manifest["common_memory_requirements"]:
                    expected_links[f"R{number}"].add(case["id"])
        else:
            assert (ROOT / case["artifact"]).is_file(), case["artifact"]
    assert covered == defined, sorted(defined - covered)
    lifecycle = {case.get("lifecycle") for case in cases}
    assert {"returned", "moved", "nested", "sliced", "unnamed",
            "owning elements"} <= lifecycle
    planned = set(re.findall(r"^\| (T\d+) \|", requirements, re.MULTILINE))
    assert planned == {case["id"] for case in cases}, "test table and manifest differ"
    for case in cases:
        row = next(line for line in requirements.splitlines()
                   if line.startswith(f"| {case['id']} |"))
        primary = {int(number) for number in re.findall(r"\bR(\d+)\b", row.rsplit("|", 2)[-2])}
        assert primary == set(case["requirements"]), case["id"]
        artifact = "test/system/" + case["source"] if "source" in case else case["artifact"]
        assert f"]({artifact})" in row, (case["id"], artifact)
        if case["kind"] == "ir":
            assert f"[{case['id']}]" in (ROOT / artifact).read_text(), case["id"]
    trace = dict(re.findall(r"^\| (R\d+) \| (.+) \|$", requirements, re.MULTILINE))
    assert set(trace) == defined, "incomplete requirement traceability table"
    programs = {case["id"] for case in cases if "source" in case}
    memory = {case["id"] for case in cases
              if "source" in case and case["kind"] != "runtime"}
    for requirement, row in trace.items():
        linked = set(re.findall(r"\[(T\d+)\]", row))
        if "[All programs]" in row:
            linked.update(programs)
        if "[Memory runner]" in row:
            linked.update(memory)
        assert linked == expected_links[requirement], requirement


def check_execution(case: dict, process: subprocess.CompletedProcess) -> str:
    """An empty result means success; a compiler/link failure never reaches here."""
    diagnostic = process.stdout + "\n" + process.stderr
    if SANITIZER_REPORT.search(diagnostic):
        return "sanitizer report:\n" + diagnostic
    if process.returncode < 0:
        return f"terminated by signal {-process.returncode}:\n{diagnostic}"
    if case["kind"] != "runtime":
        if process.returncode != 0:
            return f"exit {process.returncode}:\n{diagnostic}"
        if process.stdout != "Program output: 0\n":
            return "unexpected successful-program output:\n" + diagnostic
        return ""
    if process.returncode == 0:
        return "expected nonzero runtime exit:\n" + diagnostic
    category = re.escape(case["category"])
    if not re.search(r"\b" + category + r"\b", diagnostic):
        return f"missing {case['category']} diagnostic:\n{diagnostic}"
    actual = [int(value) for value in OUTPUT_LINE.findall(process.stdout)]
    if actual != case["before"]:
        return (f"expected pre-failure outputs {case['before']}, got {actual}; "
                "evaluation was missing, reordered, or continued after failure:\n" + diagnostic)
    return ""


def run_command(command: list[str], timeout: int,
                env: dict | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(command, capture_output=True, text=True,
                          timeout=timeout, env=env)


def sysroot_flags() -> list[str]:
    if sys.platform != "darwin":
        return []
    sdk = os.environ.get("SDKROOT")
    if not sdk:
        sdk = subprocess.check_output(
            ["xcrun", "--show-sdk-path"], text=True).strip()
    return ["-isysroot", sdk]


def run_case(case: dict, mode: str, settings: Settings,
             scratch: Path) -> list[Result]:
    start = time.monotonic()
    name = f"{case['id']}.{mode}"
    work = scratch / name
    work.mkdir()
    source = SYSTEM / case["source"]
    stage = "semantic analysis" if mode == "semantic" else "compile"
    try:
        if mode == "semantic":
            command = [str(settings.compiler), "--ptype", str(source)]
        else:
            flags = ["-do"] if mode == "unoptimized" else []
            if settings.sanitize:
                flags.append("--san")
            command = [str(settings.compiler), *flags, str(source),
                       "-o", str(work / "program.bc")]
        process = run_command(command, settings.timeout)
        if process.returncode:
            return [Result(name, False, f"{stage} failed:\n{process.stdout}{process.stderr}",
                           time.monotonic() - start)]
        if mode == "semantic":
            return [Result(name, True, duration=time.monotonic() - start)]

        stage = "link"
        flags = ["-fsanitize=address"] if settings.sanitize else []
        process = run_command(
            [settings.clang, *flags, *sysroot_flags(), str(work / "program.bc"),
             str(settings.runtime), "-o", str(work / "program")], settings.timeout)
        if process.returncode:
            return [Result(name, False, f"link failed:\n{process.stdout}{process.stderr}",
                           time.monotonic() - start)]

        stage = "execution"
        env = dict(os.environ)
        if settings.sanitize:
            env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", "") + ":detect_leaks=1:halt_on_error=1"
            env["LSAN_OPTIONS"] = env.get("LSAN_OPTIONS", "") + ":exitcode=23"
        results = []
        for number, arguments in enumerate(case.get("runs", [[]]), 1):
            process = run_command([str(work / "program"), *arguments],
                                  settings.timeout, env)
            message = check_execution(case, process)
            label = name if case["kind"] != "runtime" else f"{name}.{number}"
            results.append(Result(label, not message, message,
                                  time.monotonic() - start))
        return results
    except (OSError, subprocess.SubprocessError) as error:
        return [Result(name, False, f"{stage}: {error}", time.monotonic() - start)]


def write_junit(results: list[Result], destination: Path) -> None:
    suite = ET.Element("testsuite", name="SOP D4", tests=str(len(results)),
                       failures=str(sum(not result.passed for result in results)))
    for result in results:
        test = ET.SubElement(suite, "testcase", name=result.name,
                             time=f"{result.duration:.3f}")
        if not result.passed:
            ET.SubElement(test, "failure").text = xml_safe_text(result.message)
    destination.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(suite).write(destination, encoding="unicode", xml_declaration=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=["all", "runtime", "memory"], default="all")
    parser.add_argument("--semantic-only", action="store_true")
    parser.add_argument("--san", action="store_true")
    parser.add_argument("--mode", choices=["both", "optimized", "unoptimized"], default="both")
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--check", action="store_true", help="check fixtures and requirement coverage")
    parser.add_argument("--junit", type=Path)
    parser.add_argument("--jobs", "-j", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()
    if args.check:
        check_manifest()
        print("D4 manifest, requirement coverage, and all six lifecycle rows are consistent.")
        return 0

    compiler = Path(os.environ.get("TOPC", str(ROOT / "build/src/topc"))).resolve()
    runtime = Path(os.environ.get("RTLIB", str(compiler.parent.parent / "rtlib")))
    settings = Settings(compiler, os.environ.get("TOPCLANG", ""),
                        runtime / "top_rtlib.bc", args.san or args.suite == "memory",
                        args.timeout)
    if not compiler.is_file():
        parser.error(f"compiler not found: {compiler}")
    if not args.semantic_only:
        if not settings.clang:
            parser.error("TOPCLANG must name the matching clang")
        if not settings.runtime.is_file():
            parser.error(f"runtime bitcode not found: {settings.runtime}")
    cases = program_cases(args.suite)
    if args.case:
        selected = set(args.case)
        unknown = selected - {case["id"] for case in cases}
        if unknown:
            parser.error("unknown or excluded test IDs: " + ", ".join(sorted(unknown)))
        cases = [case for case in cases if case["id"] in selected]
    modes = ["semantic"] if args.semantic_only else (
        ["optimized", "unoptimized"] if args.mode == "both" else [args.mode])
    results = []
    with tempfile.TemporaryDirectory(prefix="sop-d4-") as directory:
        scratch = Path(directory)
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            futures = [pool.submit(run_case, case, mode, settings, scratch)
                       for case in cases for mode in modes]
            for future in concurrent.futures.as_completed(futures):
                for result in future.result():
                    results.append(result)
                    print(("PASS " if result.passed else "FAIL ") + result.name)
                    if result.message:
                        print(result.message.rstrip())
    results.sort(key=lambda result: result.name)
    if args.junit:
        write_junit(results, args.junit)
    passed = sum(result.passed for result in results)
    print(f"{passed}/{len(results)} D4 checks passed")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
