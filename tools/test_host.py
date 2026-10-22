"""GCC host fallback for environments without CMake (same N7 source selection)."""
import pathlib
import re
import subprocess
ROOT = pathlib.Path(__file__).resolve().parents[1]
build = ROOT / 'build-n7-host'
build.mkdir(exist_ok=True)
n7 = ROOT / 'third_party/n7space'
cmake = (n7 / 'CMakeLists.txt').read_text()
sources = []
for names, folder in re.findall(r'foreach\(file ([^)]+)\)\s+list\(APPEND n7_sources lib/(\w+)/', cmake):
    sources.extend(n7 / 'lib' / folder / (name + '.c') for name in names.split())
defines = re.findall(r'\b(?:LELY_\w+|CO_NMT_MAX_NHB)=\d+', cmake)
common = ['gcc', '-std=c11', '-g', '-O1', '-fsanitize=address,undefined',
          '-fno-omit-frame-pointer', '-I' + str(n7 / 'include')]
common += ['-D' + d for d in defines]
objects = []
for source in sources:
    obj = build / (source.parent.name + '_' + source.stem + '.o')
    if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, (n7 / 'CMakeLists.txt').stat().st_mtime):
        subprocess.run(common + ['-c', str(source), '-o', str(obj)], check=True)
    objects.append(str(obj))
own = [p for p in (ROOT / 'src').rglob('*.c')]
own += list((ROOT / 'tests').glob('*.c')) + list((ROOT / 'tests/mocks').glob('*.c'))
own += list((ROOT / 'generated/can').glob('*.c'))
command = common + ['-Wall', '-Wextra', '-Wpedantic', '-Werror']
command += ['-I' + str(ROOT / path) for path in ('include', 'tests', 'generated/can', 'generated/canopen')]
subprocess.run(command + [str(p) for p in own] + objects + ['-o', str(build / 'itm_tests')], check=True)
subprocess.run([str(build / 'itm_tests')], check=True)
