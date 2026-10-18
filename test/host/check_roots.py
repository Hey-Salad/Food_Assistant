#!/usr/bin/env python3
"""Confirm the embedded uplink CA bundle is the public roots we claim."""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "src" / "tls_roots.cpp").read_text()
pems = re.findall(
    r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
    header,
    flags=re.S,
)
expected = [
    "ISRG Root X1",
    "ISRG Root X2",
    "GTS Root R1",
    "GTS Root R2",
    "GTS Root R3",
    "GTS Root R4",
]
if len(pems) != len(expected):
    sys.exit(f"expected {len(expected)} certificates, found {len(pems)}")

subjects = []
for pem in pems:
    with tempfile.NamedTemporaryFile("w", delete=False) as handle:
        handle.write(pem + "\n")
        name = handle.name
    result = subprocess.run(
        ["openssl", "x509", "-in", name, "-noout", "-subject"],
        check=True,
        capture_output=True,
        text=True,
    )
    subjects.append(result.stdout.strip())

for name, subject in zip(expected, subjects):
    if name not in subject:
        sys.exit(f"missing {name} in {subject}")

live = (ROOT / "src" / "main.cpp").read_text().split("#endif", 1)[1]
for forbidden in ('server.on("/get_keys"', 'server.on("/save_keys"', "setInsecure("):
    if forbidden in live:
        sys.exit(f"live server still contains {forbidden}")

chunks = []
for path in list((ROOT / "src").glob("*.cpp")) + list((ROOT / "include").glob("*.h")):
    for line in path.read_text().splitlines():
        code = line.split("//", 1)[0]
        chunks.append(code)
if "setInsecure(" in "\n".join(chunks):
    sys.exit("setInsecure( is still called")

print("tls roots: " + ", ".join(expected))
