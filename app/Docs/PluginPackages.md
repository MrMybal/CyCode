# Signed local plugin packages

Extensions > Plugin packages installs, updates and removes local native-extension
packages. This installation-wide selection is separate from the existing
per-project enable/disable settings. Six bundled/legacy extensions keep their
existing discovery and app-update mechanism; managed packages cannot replace
their IDs. The manager does not download packages or operate a marketplace.

## Desktop workflow

Choose a package folder or enter its path, then Inspect package. Inspection verifies
the signature and all payload hashes without loading native code. It shows the
publisher fingerprint, requested host-service permissions, version, target platform
and compatibility. Publisher labels are local names, not verified organization names.
Verify the fingerprint independently before explicitly enrolling a new publisher.
The trust checkbox and Trust publisher button only enroll the key; installation
requires a separate Install / update package action after the refreshed inspection.

Only signed packages from already trusted publishers can be installed. Updates
must increase the numeric version and keep the same publisher key. A publisher
change requires explicit removal followed by a fresh installation. The selected
version and four previous versions are retained. Select previous version verifies
the retained payload, publisher, host and dependency constraints before rollback.
Removing a package checks required dependents before changing the selection.

Selection changes take effect for new application processes. Existing hosts keep
their pinned versions, including when switching workspaces in the same process;
there is no DLL hot replacement. The UI compares its actual
discovered versions against the current selection and reports the difference.
Revoking a publisher blocks future process discovery but cannot remove already
loaded native code from an existing process. Per-project activation stays separate.

Unused versions can be deleted after they leave the selection/rollback history.
The manager takes an exclusive native file lease; a version pinned by another
host remains untouched with `action:inUse`. Deletion uses verified exact file paths,
never recursive directory deletion. Unverifiable packages are reported for manual
inspection. Cleanup is best effort when the OS denies access; unknown files are
not swept. Package directories have a shortened content address for path length,
while the complete signed identity is checked against the index before use.

## Persistence and transactions

The writable portable installation owns `extensions-managed/`, excluded from Git
and release archives. It contains index.json, publishers.json, immutable package
directories, staging and native lease markers. Do not store it in an SDK task
workspace: installation is explicit persistent application state.

Global shared/exclusive OS leases coordinate index/trust reads and writes across
processes. Byte-baseline atomic writes and SHA-512 revision tokens reject stale
edits. Copying occurs outside the index lock; commit checks both revisions again.
Files are copied exclusively into an owned staging directory and checked while
streaming. A failed unpublished copy attempts exact-file cleanup, with directory
identity checks. A successfully published version whose selection commit fails is
an unused package, available for guarded cleanup. No existing version is overwritten.
An ancillary catalogue error is reported separately from a successful selection
commit. The existing UI-free manifest validator is reused by the loader and manager.

The loader freezes a verified selection on its first installation discovery in
each process, with shared leases even for dormant packages. It verifies signatures,
platform and trust before discovery, and rechecks the complete payload and signed
identity before each first library initialization in a host. Switching workspaces
cannot adopt an update prepared after process startup. Managed version leases
outlive library use and initialization failures, and the process snapshot retains
them until process exit. An installation with no managed store requires no store
creation during ordinary plugin startup. On POSIX, new owned store directories
use owner-only access; actual Linux/macOS behavior remains untested.
Native plugins remain trusted code with the account's OS privileges. Signatures
authenticate publisher/content; they are not process isolation or a filesystem
sandbox. OS-level isolation of untrusted extensions remains separate work.

## Package format

A package is a directory containing package.json, signature.json and the explicitly
listed payload. No archive extraction or install script runs. package.json contains:

```json
{
  "schemaVersion": 1,
  "platform": "windows-x64",
  "plugin": {
    "id": "example.tools",
    "name": "Example tools",
    "version": "1.0.0",
    "abi": 2,
    "library": "example_tools",
    "permissions": ["workspace.write"]
  },
  "files": [
    {"path": "example_tools.dll", "size": 1234, "sha512": "<128 lowercase hexadecimal characters>", "executable": false}
  ]
}
```

The manifest uses the [native extension SDK](Plugins.md), including optional host
version bounds, supported platforms and dependencies. Managed IDs are portable
lowercase identifiers up to 100 bytes. The platform names the actual process
OS/architecture. Its declared `.dll`, `.so` or `.dylib` library must be listed.
The package supports up to 32 permissions and 32 dependencies, with known dependency
fields only. Names are bounded; metadata never supplies runtime host state.

signature.json contains algorithm `Ed25519`, a 32-byte publicKey and a 64-byte
signature, both lowercase hexadecimal. The signed message is the exact concatenation
of UTF-8 `CyCode plugin package v1` followed by LF and the raw package.json bytes.
This is ordinary Ed25519, not Ed25519ph. The full package identity is SHA-512 of
`CyCode package identity v1`, LF, the hexadecimal public key, LF and those raw bytes.
File checksums use SHA-512 of file bytes. Whitespace changes require a new signature.
Duplicate JSON keys, excessive nesting, symlinks/Windows reparse points, path traversal, alternate data
streams, portable-name collisions and unexpected installed files/directories are
refused. The host-generated manifest must match the signed declaration exactly.

Limits: 256 KiB package metadata, 2 KiB signature envelope, 256 files, 64 MiB per
file and 256 MiB total; paths up to 1024 UTF-8 bytes, 32 components and 160 bytes
per component. Actual OS/path limits still apply. Up to 64 selected packages and
128 publisher records are accepted. Dependency traversal is limited to 128 edges
per path. The catalogue scans at most 4096 stored entries,
returns 128 unused versions at a time and bounds storage-integrity diagnostics.
Native initialization and external runtime dependencies still need real validation;
matching metadata does not prove that a library exports the required ABI correctly.

## Authoring

`app/scripts/package_plugin.py` creates a package from an existing native build,
an SDK manifest and explicit `--file` selections. Its optional authoring dependency
is Python `cryptography`; the application itself uses pinned Monocypher 4.0.3.
Supply an existing Ed25519 PKCS8 PEM private key outside the payload/output folders.
No key is generated, downloaded or included. `--ask-password` reads an encrypted
key's password interactively. Keep signing keys outside repositories and payloads.

```sh
python app/scripts/package_plugin.py --manifest plugin.json --payload build/Release --file example_tools.dll --file LICENSE --platform windows-x64 --private-key ../publisher-keys/publisher.pem --output dist/example-tools-1.0.0
```

Use repeated `--file` arguments for assets/notices and `--executable path` for
cross-platform helper permissions. The output must be a new directory outside the
payload. The script does not load code. The native host remains the authoritative
manifest/platform/ABI/dependency validator. Authoring failures may leave an incomplete
output directory for inspection; it cannot be installed without valid metadata,
signature and payload. Include the extension's own necessary license notices.

## MCP and verification

`packages_list/inspect/install/remove/rollback/clean` share the native service.
Inspection/installation sources remain workspace directories; the desktop picker
provides explicit access to folders elsewhere. Publisher enrollment/revocation is
only exposed by the desktop, with no MCP trust-mutation operation. Read operations
require `plugins.execute`; mutations also require `process.execute` and
`workspace.write`. Install needs expectedRevision, expectedTrustRevision and
expectedPackage from inspection. Rollback needs both state revisions; remove/clean
need the selection revision. Results describe persistent installed state, not
temporary task files.

The SDK declares these size-bounded filesystem transactions noncancellable and uses a
separate plugin-packages exclusivity group for mutations. Read-only catalogue and
inspection jobs do not take the workspace-mutations group. The bridge waits for
the native result; after a transport/OS failure refresh the catalogue before retrying.
No process runner, scheduler, identity or MCP implementation is duplicated.

Monocypher's official release archive was checked against its published SHA-512,
then SHA-256 pinned in dependencies.lock.json. Its full notice is in
licenses/Monocypher.txt. Source-only authoring path/duplicate-key checks and real
SDK/schema checks passed. Native signature/install/update/rollback/concurrency and
GUI acceptance remain unexecuted during the antivirus interruption. No signing
key, plugin package, test DLL/executable or app window was created/launched for
these checks. Windows production compilation is recorded in [Validation](Validation.md);
Linux/macOS and clean-machine installation remain untested.

Primary contracts: [Monocypher Ed25519](https://monocypher.org/manual/ed25519),
[SHA-512](https://monocypher.org/manual/sha512),
[official releases/checksums](https://monocypher.org/download/).
