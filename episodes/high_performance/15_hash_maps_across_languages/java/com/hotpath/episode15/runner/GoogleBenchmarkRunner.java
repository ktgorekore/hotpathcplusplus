// Copyright 2026 Hot Path C++ Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.hotpath.episode15.runner;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import org.openjdk.jmh.results.RunResult;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.RunnerException;
import org.openjdk.jmh.runner.options.ChainedOptionsBuilder;
import org.openjdk.jmh.runner.options.CommandLineOptionException;
import org.openjdk.jmh.runner.options.CommandLineOptions;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;
import org.openjdk.jmh.runner.options.VerboseMode;

/**
 * Custom JMH runner formatting Episode 15 Java benchmarks as a Google Benchmark table.
 */
public final class GoogleBenchmarkRunner {

  private static final Logger logger = LogManager.getLogger(GoogleBenchmarkRunner.class);
  private static final Logger tableLogger =
      LogManager.getLogger("com.hotpath.episode15.runner.TableLogger");

  private GoogleBenchmarkRunner() {}

  public static void main(String[] args) {
    boolean enableColor = true;
    boolean rawJmh = false;
    boolean userSpecifiedVerbosity = false;

    if (System.getenv("NO_COLOR") != null && !System.getenv("NO_COLOR").isEmpty()) {
      enableColor = false;
    }

    List<String> jmhArgsList = new ArrayList<>();
    for (String arg : args) {
      if ("--no-color".equalsIgnoreCase(arg)) {
        enableColor = false;
      } else if ("--raw-jmh".equalsIgnoreCase(arg)) {
        rawJmh = true;
      } else {
        if ("-v".equals(arg) || arg.startsWith("-v=")) {
          userSpecifiedVerbosity = true;
        }
        jmhArgsList.add(arg);
      }
    }

    CommandLineOptions cmdOptions;
    try {
      cmdOptions = new CommandLineOptions(jmhArgsList.toArray(new String[0]));
    } catch (CommandLineOptionException e) {
      logger.error("Error parsing benchmark CLI args: {}", e.getMessage());
      return;
    }

    if (cmdOptions.shouldHelp()) {
      try {
        cmdOptions.showHelp();
      } catch (java.io.IOException e) {
        logger.error("Failed to display JMH help", e);
      }
      return;
    }

    if (cmdOptions.shouldList()) {
      new Runner(cmdOptions).list();
      return;
    }

    ChainedOptionsBuilder optionsBuilder = new OptionsBuilder().parent(cmdOptions);
    if (!rawJmh && !userSpecifiedVerbosity) {
      optionsBuilder.verbosity(VerboseMode.SILENT);
    }

    Options options = optionsBuilder.build();
    if (!rawJmh) {
      tableLogger.info("Executing Episode 15 JMH benchmarks...");
    }

    Collection<RunResult> results;
    try {
      results = new Runner(options).run();
    } catch (RunnerException e) {
      logger.error("Fatal error during JMH execution", e);
      return;
    }

    if (!rawJmh) {
      GoogleBenchmarkTablePrinter printer = new GoogleBenchmarkTablePrinter(enableColor);
      printer.printReport(results);
    }
  }
}
