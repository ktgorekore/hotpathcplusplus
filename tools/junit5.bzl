# Copyright 2026 Hot Path C++ Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Starlark macro for defining hermetic JUnit 5 tests via ConsoleLauncher."""

load("@rules_java//java:defs.bzl", "java_test")

def junit5_test(name, srcs, test_class, deps = [], runtime_deps = [], **kwargs):
    """Defines a hermetic JUnit 5 test using org.junit.platform.console.ConsoleLauncher.

    Args:
        name: Name of the test target.
        srcs: Test source files.
        test_class: Fully-qualified name of the test class to execute.
        deps: Compile-time dependencies for the test.
        runtime_deps: Runtime dependencies for the test.
        **kwargs: Additional parameters passed through to java_test.
    """
    java_test(
        name = name,
        srcs = srcs,
        use_testrunner = False,
        main_class = "org.junit.platform.console.ConsoleLauncher",
        args = [
            "execute",
            "--select-class=" + test_class,
            "--fail-if-no-tests",
        ],
        deps = deps + [
            "@maven//:org_junit_jupiter_junit_jupiter_api",
            "@maven//:org_junit_jupiter_junit_jupiter_params",
            "@maven//:org_assertj_assertj_core",
            "@maven//:org_awaitility_awaitility",
        ],
        runtime_deps = runtime_deps + [
            "@maven//:org_junit_jupiter_junit_jupiter_engine",
            "@maven//:org_junit_platform_junit_platform_launcher",
            "@maven//:org_junit_platform_junit_platform_console",
        ],
        **kwargs
    )
