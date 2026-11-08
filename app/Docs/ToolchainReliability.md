# Tool discovery and build diagnostics

`cycode_process` owns the same tool discovery for the IDE, native services and
bundled providers. Environment overrides, packaged tools and absolute PATH entries
keep their existing priority. Discovered candidates must be regular files and,
on POSIX, have execute permission. Relative PATH entries remain excluded.

CMake detection now uses this owner too, through `findExecutableOnPath`. Its
PATH-first, explicit-hints-second order is preserved; managed tool overrides
remain specific to `toolPath`. Quoted absolute PATH entries work for both clients.
The former CMake-local resolver canonicalized compiler aliases independently of
the managed-tool resolver, so correcting only the latter did not fix generated
C++ project builds.

Discovery preserves the candidate's original path rather than canonicalizing
symbolic links. This matters for `clang++`/`g++`, which choose the C++ link runtime
using their invocation name, and for environment-specific interpreter aliases.
An explicit absolute tool argument retains its existing pass-through behavior;
discovery is not a guarantee that a subsequent launch will succeed.

Compiler-cache comparisons retain the invocation filename while resolving parent
directory aliases. A saved `clang` driver no longer matches a newly selected
`clang++` merely because both links point at the same executable. Existing source/
build-tree containment checks still use their normal filesystem identities.
Refresh toolchain detection and reselect an older saved canonical toolchain ID;
choose a new build directory when its recorded driver differs. Automatic/custom
or preset-based configurations are not silently rewritten.

The build wrapper prints compiler errors with adjacent source/notes, including
Clang diagnostics that the previous `FAILED:`-only summary could hide. Its failure
preview reads the final 512 KiB, selects at most 80 lines and prints at most 12,000
characters. The full log path remains available. If a reason occurs only earlier
in a larger log, open that file; the preview does not replace the complete log.

Windows CI uses `windows-2022` with the existing Visual Studio 2022 generator.
Failed core builds preserve their log as a seven-day workflow artifact. Linux and
macOS use the existing Ninja configuration. These workflow changes require a new
remote run; local Windows compilation does not validate those platforms.

Windows Release compilation maps checkout/dependency paths to relative source
paths, including the compiler's deterministic preprocessing option. This prevents
the inspected application binaries from embedding the local checkout through
`__FILE__`; it does not promise byte-identical builds or remove every possible
toolchain provenance field.

## Verification

Run data-only checks from the repository root:

```text
python app/scripts/verify_sources.py
python app/tests/test_build_diagnostics.py
python app/tests/test_release_package.py
```

The six reporting tests use text and temporary log files. The five package tests
use in-memory data; neither suite launches native programs. The opt-in native
core suite also specifies symlink preservation, broken-link rejection and POSIX
execute-permission checks. Its bodies remain unexecuted locally while the current
antivirus removals are unexplained. See [Testing](Testing.md) and
[Validation](Validation.md) for the exact current verification scope.

The opt-in core/build sources additionally specify PATH/hint precedence, quoted
entries, directory aliases, filename/case identity and real detected C++ driver
paths. MSVC and Clang syntax-only checks use the installed Windows headers and
ABI; they do not validate the POSIX branches or establish a macOS build.


## Filesystem clock compatibility

Unreal analysis and MSBuild file signatures serialize every filesystem tick as
decimal text. libc++ on macOS uses a 128-bit filesystem clock representation;
passing that count directly to std::to_string is ambiguous. The shared FileTime
helper preserves the complete value, including negative limits, without narrowing
to 64 bits or truncating subsecond changes. Existing 64-bit cache tokens retain
their representation. Production compile-time checks include values beyond 64 bits
when the compiler supports them. See the upstream
[libc++ filesystem clock design](https://libcxx.llvm.org/DesignDocs/FileTimeType.html).


## Process logs at the text boundary

Child stdout/stderr remain raw bytes inside ProcessResult and protocol parsers.
User-facing process-result JSON and live build/project tails convert a copy to
valid UTF-8. Valid UTF-8 lines remain unchanged. Malformed Windows console lines
use the system OEM code page; malformed non-Windows bytes are replaced explicitly.
This prevents a legacy compiler log from breaking JSON serialization of an
otherwise successful build, including accented workspace names. Source/blob data
and structured JSON discovery are never transcoded by this display conversion.
Mixed encodings within one line have no unambiguous automatic interpretation;
prefer tools configured for UTF-8. No global console/code-page setting is changed.
