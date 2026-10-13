"""Check affected production helpers with a host C++ compiler and syscall fixtures."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def function(path, signature):
    source = path.read_text()
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    code = r'''
#include <cassert>
#include <climits>
#include <cstring>
class StorageScope { public: ~StorageScope() {} };
static int position, convention, shortWrite, calls;
int Bfile_TellFile_OS(int) { return position; }
int Bfile_WriteFile_OS(int, const void*, int size) {
  calls++;
  if (shortWrite < 0) return -1;
  position += size - shortWrite;
  return convention ? position : size - shortWrite;
}
static int painted, simulated, localized;
void mPrintMiniMiniRawAt(int* x, int*, const char* s, int, int, int simulate) {
  *x += (int)strlen(s) * 6;
  if (simulate) simulated++; else painted++;
}
void mPrintMiniMiniAt(int* x, int* y, const char* s, int m, int c, int simulate) {
  localized++;
  mPrintMiniMiniRawAt(x, y, s, m, c, simulate);
}
'''
    code += function(root / "src/platformProvider.cpp", "static unsigned short decodeVersionBCD")
    code += function(root / "src/storageProvider.cpp", "int storageWriteFile")
    code += function(root / "src/graphicsProvider.cpp", "static int measureMiniMiniWidth")
    code += function(root / "src/stringsProvider.cpp", "void formatHex32")
    code += function(root / "src/stringsProvider.cpp", "int isChineseText")
    code += r'''
int main() {
  char hex[10] = "xxxxxxxxx";
  hex[9] = '!';
  formatHex32(hex, 0);
  assert(!strcmp(hex, "00000000") && hex[9] == '!');
  formatHex32(hex, 0xAF123456);
  assert(!strcmp(hex, "AF123456"));
  assert(!strcmp(hex + 4, "3456"));
  formatHex32(hex, 0xffffffff);
  assert(!strcmp(hex, "FFFFFFFF"));
  formatHex32(nullptr, 0);
  assert(!isChineseText(nullptr));
  assert(!isChineseText(""));
  assert(!isChineseText("2026-10-11"));
  assert(isChineseText("\xc4\xea"));
  assert(isChineseText("2026\xc4\xea"));
  assert(isChineseText("12: \xc4\xea"));
  assert(!isChineseText("\xe5\x9f"));
  assert(!isChineseText("A \xf7\x20"));
  assert(decodeVersionBCD(0x03) == 3);
  assert(decodeVersionBCD(0x81) == 81);
  assert(decodeVersionBCD(0x0202) == 202);
  assert(decodeVersionBCD(0x9999) == 9999);
  assert(decodeVersionBCD(0) == 0);
  assert(decodeVersionBCD(0x01a2) == 0);
  assert(decodeVersionBCD(0xf000) == 0);
  char buffer[128] = {};
  for (convention = 0; convention < 2; convention++) {
    position = 0; shortWrite = 0;
    assert(storageWriteFile(1, buffer, 128));
    assert(storageWriteFile(1, buffer, 128));
    assert(position == 256);
    shortWrite = 1;
    assert(!storageWriteFile(1, buffer, 128));
    shortWrite = -1;
    assert(!storageWriteFile(1, buffer, 128));
  }
  calls = 0; position = -1;
  assert(!storageWriteFile(1, buffer, 1));
  position = INT_MAX;
  assert(!storageWriteFile(1, buffer, 1));
  assert(!storageWriteFile(-1, buffer, 1));
  assert(!storageWriteFile(1, nullptr, 1));
  assert(!storageWriteFile(1, buffer, 0));
  assert(calls == 0);
  assert(measureMiniMiniWidth("SAVE", 0, 0, 0) == 24);
  assert(measureMiniMiniWidth("", 0, 0, 1) == 0);
  assert(painted == 0 && simulated == 2 && localized == 1);
}
'''
    environment = os.environ.copy()
    environment["PATH"] = str(args.cxx.resolve().parent) + os.pathsep + environment["PATH"]
    with tempfile.TemporaryDirectory(prefix="utilities-regression-") as directory:
        source = Path(directory) / "check.cpp"
        binary = Path(directory) / "check.exe"
        source.write_text(code)
        subprocess.run([str(args.cxx), "-std=c++11", "-Wall", "-Wextra", str(source), "-o", str(binary)], check=True, env=environment)
        subprocess.run([str(binary)], check=True, env=environment)
    print("RUNTIME_REGRESSION=pass (BCD, write progress, draw-free measurement, hex, mixed Chinese)")


if __name__ == "__main__":
    main()
