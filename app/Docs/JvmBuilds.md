# Maven and Gradle project tasks

The shared native ProjectTasks adapter now detects `pom.xml`, `build.gradle`,
`build.gradle.kts` and settings-only Gradle roots before standalone Java fallback.
The existing panel, build/test/run shortcuts, configured deadlines, process-tree
cancellation, persistent profile and SDK/MCP operations use these adapters.
Inspection only reads descriptors/cache; it does not evaluate rules, resolve
dependencies, execute wrappers or install tools.

## Tool selection

Install a suitable JDK and build tool, or supply the project's existing wrapper
jar. `CYCODE_JAVA`/PATH selects Java using existing tool discovery. Explicit
`CYCODE_MAVEN_HOME` or `CYCODE_GRADLE_HOME` selects an installed distribution.
Otherwise the adapter searches ancestors within the workspace for
`.mvn/wrapper/maven-wrapper.jar` or `gradle/wrapper/gradle-wrapper.jar`; installed
MAVEN_HOME/M2_HOME/GRADLE_HOME is the fallback. Missing jars/tools produce errors.
No wrapper jar or distribution is downloaded by opening/inspecting a project.

Explicit Configure/Build/Test/Run executes trusted Java build tooling and project
rules, which may download distributions/plugins/dependencies as usual. Offline mode
passes the tool's `--offline`; it does not promise that an uncached wrapper
distribution is available offline. CyCode invokes wrapper main classes or installed
Java launchers with argv vectors, never a project-derived shell command. Maven 3
uses its ClassWorlds bootstrap; Gradle supports the older launcher and newer
gradle-gradle-cli-main layouts. Other layouts require adaptation/runtime acceptance.

Shell startup files, MAVEN_OPTS/JAVA_OPTS and shell parsing of `.mvn/jvm.config`
are not reproduced. The selected JVM honors its own supported environment options;
project build configuration remains owned by Maven/Gradle. If a Maven wrapper
requests shell-side `wrapperSha256Sum`, the adapter refuses to bypass that check:
use the official wrapper externally or select an installed Maven distribution
explicitly. Jarless/script-only wrapper distributions need an installed tool here.

## Tasks and runtime models

| Operation | Maven | Gradle |
| --- | --- | --- |
| Configure | Nonrecursive effective POM through pinned Maven Help Plugin 3.5.2 | Application-owned init task exports actual Java source-set model as bounded JSON |
| Build | `package -DskipTests` | `build` with normal project test/dependency semantics |
| Test | `test`, using project framework configuration | `test`, using project framework configuration |
| Run build task | One simple goal/phase such as clean/verify/install; preserves its test semantics | One simple task, optionally scoped to a selected module |
| Run | Build, resolve runtime dependencies through Maven Dependency Plugin 3.11.0, then run the selected actual main class | Build, export runtime source-set classpath, then run the selected actual main class |

Gradle's package field selects a module (`app` normalizes to `:app`; empty builds
all). Maven selects its module through Project directory. A reactor-root package
build may build its modules, but runtime metadata describes the selected POM only;
choose a runnable module directory and ensure reactor dependencies are available.
This is not a complete Maven reactor dependency/import model.

Runnable entry points are read from actual compiled class-file main methods, not
source regexes. Class output/build paths are workspace-guarded. Runtime dependency
classpath entries may reference installed external jars; they are runtime inputs,
not newly editable workspace documents. Gradle Java source sets supply their actual
runtimeClasspath; Maven uses its effective output directory and resolved runtime
dependencies. A failed/incomplete operation does not publish runnable artifacts.

Java Run uses a fresh quoted argument file with disabled further @file expansion,
preserving empty arguments, quotes, line breaks and shell punctuation while
avoiding Windows command-line length limits. This launch path requires Java 9+;
UTF-8 argument-file behavior on supported JDK/platform combinations still needs
runtime verification. Java module-path applications and custom main launchers are
not inferred from a classpath. Debug/Release labels do not invent project-specific
Maven profiles or Gradle variants.

Project profiles include timeoutSeconds (1–86400; default 180). This is one total
process budget across build/model/dependency steps. The GUI exposes it and the SDK
retains ownership/cancellation while waiting for a saved long deadline. Increase
it explicitly for a first large dependency resolution. Results/logs remain bounded;
source/class scanning has entry/depth/class/artifact limits and observes cancellation.

## Validation and remaining work

Windows production compilation and MSVC `/Zs` source checks cover the adapter,
panel and model/argument regression sources. Regressions specify Maven XML entities
and unresolved properties, Gradle modules/outside outputs/dependencies, classpath
bounds, exact argument-file encoding, wrapper argv and checksum refusal. They are
added to the existing opt-in project test executable, without a new test binary.

No Java, Maven, Gradle, wrapper, generated Java fixture, test executable, debugger
or app window was launched for this change during the Kaspersky interruption.
Actual builds/tests/dependency resolution/interactive output, client/JDK versions,
cross-module runtime behavior and GUI acceptance remain pending. Linux/macOS and
clean-machine installation are untested. A subsequent implementation connects
launch debugging through JDT LS; see [Java debugging](JavaDebugging.md) for setup
and separately pending acceptance. This does not claim
per-test JUnit/TestNG result discovery, Android/JPMS support, Maven reactor parity
or full Gradle Tooling API coverage.

References: [Maven Wrapper](https://maven.apache.org/wrapper/),
[effective POM](https://maven.apache.org/plugins/maven-help-plugin/effective-pom-mojo.html),
[runtime classpath](https://maven.apache.org/plugins/maven-dependency-plugin/build-classpath-mojo.html),
[Gradle initialization](https://docs.gradle.org/current/userguide/init_scripts.html),
[source sets](https://docs.gradle.org/current/dsl/org.gradle.api.tasks.SourceSet.html),
[Java argument files](https://docs.oracle.com/en/java/javase/24/docs/specs/man/java.html).
