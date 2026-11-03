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

package com.hotpath.episode13.runner;

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.OperatingSystemMXBean;
import java.lang.management.RuntimeMXBean;
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Collection;
import java.util.List;
import java.util.stream.Collectors;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import org.openjdk.jmh.infra.BenchmarkParams;
import org.openjdk.jmh.results.Result;
import org.openjdk.jmh.results.RunResult;

/**
 * Formats and prints OpenJDK JMH microbenchmark results into a Google Benchmark-style tabular
 * layout.
 *
 * <p><b>Thread Safety Guarantee:</b> This class is stateless and {@code Thread-Safe}. Output
 * operations are dispatched sequentially via Log4j 2.
 *
 * <p><b>Memory Consistency:</b> Immutable configuration state ({@code enableColors}).
 */
public final class GoogleBenchmarkTablePrinter {

  private static final Logger tableLogger =
      LogManager.getLogger("com.hotpath.episode13.runner.TableLogger");

  private static final String RESET = "\u001B[0m";
  private static final String BOLD = "\u001B[1m";
  private static final String GREEN = "\u001B[32m";
  private static final String WHITE = "\u001B[97m";
  private static final String GRAY = "\u001B[90m";

  private final boolean enableColors;

  /**
   * Constructs a {@code GoogleBenchmarkTablePrinter} with the specified ANSI color preference.
   *
   * @param enableColors {@code true} to render ANSI colored terminal output, {@code false} for
   *     plain ASCII
   */
  public GoogleBenchmarkTablePrinter(boolean enableColors) {
    this.enableColors = enableColors;
  }

  /**
   * Prints the complete benchmark execution report, including system context header and metrics
   * table.
   *
   * @param results collection of benchmark execution results from JMH
   */
  public void printReport(Collection<RunResult> results) {
    if (results == null || results.isEmpty()) {
      tableLogger.info("No benchmark results to display.");
      return;
    }

    printSystemContext();
    printTable(results);
  }

  private void printSystemContext() {
    OperatingSystemMXBean osBean = ManagementFactory.getOperatingSystemMXBean();
    RuntimeMXBean runtimeBean = ManagementFactory.getRuntimeMXBean();
    MemoryMXBean memoryBean = ManagementFactory.getMemoryMXBean();
    List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();

    String timestamp = ZonedDateTime.now().format(DateTimeFormatter.ISO_OFFSET_DATE_TIME);
    int cpus = osBean.getAvailableProcessors();
    String osName = osBean.getName();
    String osArch = osBean.getArch();
    String vmName = runtimeBean.getVmName();
    String vmVersion = runtimeBean.getVmVersion();
    String vmArgs = String.join(" ", runtimeBean.getInputArguments());

    String gcNames =
        gcBeans.stream().map(GarbageCollectorMXBean::getName).collect(Collectors.joining(", "));

    long initHeapMb = memoryBean.getHeapMemoryUsage().getInit() / (1024 * 1024);
    long maxHeapMb = memoryBean.getHeapMemoryUsage().getMax() / (1024 * 1024);

    tableLogger.info("");
    tableLogger.info(timestamp);
    tableLogger.info("Running Episode 13 JMH Benchmarks with Google Benchmark layout");
    tableLogger.info("Run on ({} X CPU threads, {} {})", cpus, osName, osArch);
    tableLogger.info("JVM: {} (build {})", vmName, vmVersion);
    tableLogger.info("Garbage Collectors: [{}]", gcNames.isEmpty() ? "Default" : gcNames);
    tableLogger.info("Heap Memory: {} MB initial, {} MB maximum", initHeapMb, maxHeapMb);
    if (!vmArgs.isEmpty()) {
      tableLogger.info("JVM Flags: {}", vmArgs);
    }
    tableLogger.info("");
  }

  private void printTable(Collection<RunResult> results) {
    int colBenchmark = 58;
    int colTime = 16;
    int colCpu = 16;
    int colThroughput = 22;
    int colIters = 12;
    int colError = 12;

    int totalWidth = colBenchmark + colTime + colCpu + colThroughput + colIters + colError + 5;
    String separator = "-".repeat(totalWidth);

    tableLogger.info(color(separator, GRAY));

    String header =
        padRight("Benchmark", colBenchmark)
            + " "
            + padLeft("Time (ns/op)", colTime)
            + " "
            + padLeft("CPU (ns/op)", colCpu)
            + " "
            + padLeft("Throughput", colThroughput)
            + " "
            + padLeft("Iterations", colIters)
            + " "
            + padLeft("Error", colError);
    tableLogger.info(color(header, BOLD + GREEN));
    tableLogger.info(color(separator, GRAY));

    for (RunResult runResult : results) {
      BenchmarkParams params = runResult.getParams();
      Result<?> primary = runResult.getPrimaryResult();

      String benchmarkName = simplifyBenchmarkName(params.getBenchmark());
      double score = primary.getScore();
      double scoreError = primary.getScoreError();
      long sampleCount = primary.getSampleCount();

      double nsPerOp = calculateNsPerOp(score, primary.getScoreUnit());
      String throughputStr = formatThroughput(score, primary.getScoreUnit());
      String errorStr = formatError(score, scoreError);

      String row =
          padRight(benchmarkName, colBenchmark)
              + " "
              + padLeft(formatDuration(nsPerOp), colTime)
              + " "
              + padLeft(formatDuration(nsPerOp), colCpu)
              + " "
              + padLeft(throughputStr, colThroughput)
              + " "
              + padLeft(String.valueOf(sampleCount), colIters)
              + " "
              + padLeft(errorStr, colError);

      tableLogger.info(color(row, WHITE));
    }

    tableLogger.info(color(separator, GRAY));
    tableLogger.info("");
  }

  private double calculateNsPerOp(double score, String unit) {
    if (score <= 0.0 || unit == null) {
      return 0.0;
    }
    return switch (unit.toLowerCase()) {
      case "ops/s", "thrpt" -> (1_000_000_000.0 / score);
      case "ns/op" -> score;
      case "us/op" -> score * 1_000.0;
      case "ms/op" -> score * 1_000_000.0;
      case "s/op" -> score * 1_000_000_000.0;
      default -> 1_000_000_000.0 / score;
    };
  }

  private String formatDuration(double ns) {
    if (ns < 1_000.0) {
      return String.format("%.2f ns", ns);
    } else if (ns < 1_000_000.0) {
      return String.format("%.2f us", ns / 1_000.0);
    } else if (ns < 1_000_000_000.0) {
      return String.format("%.2f ms", ns / 1_000_000.0);
    } else {
      return String.format("%.2f s", ns / 1_000_000_000.0);
    }
  }

  private String formatThroughput(double score, String unit) {
    if (unit != null
        && !unit.toLowerCase().contains("ops/s")
        && !unit.toLowerCase().contains("thrpt")) {
      double ns = calculateNsPerOp(score, unit);
      if (ns > 0) {
        score = 1_000_000_000.0 / ns;
      }
    }

    if (score >= 1_000_000_000.0) {
      return String.format("%.2f Gops/s", score / 1_000_000_000.0);
    } else if (score >= 1_000_000.0) {
      return String.format("%.2f Mops/s", score / 1_000_000.0);
    } else if (score >= 1_000.0) {
      return String.format("%.2f Kops/s", score / 1_000.0);
    } else {
      return String.format("%.2f ops/s", score);
    }
  }

  private String formatError(double score, double error) {
    if (Double.isNaN(error) || error == 0.0 || score <= 0.0) {
      return "± 0.00%";
    }
    double percentage = (error / score) * 100.0;
    return String.format("± %.2f%%", percentage);
  }

  private String simplifyBenchmarkName(String fullName) {
    if (fullName == null) {
      return "";
    }
    int lastDot = fullName.lastIndexOf('.');
    if (lastDot > 0) {
      int prevDot = fullName.lastIndexOf('.', lastDot - 1);
      if (prevDot >= 0) {
        return fullName.substring(prevDot + 1);
      }
    }
    return fullName;
  }

  private String color(String text, String ansiCode) {
    return enableColors ? ansiCode + text + RESET : text;
  }

  private static String padRight(String s, int n) {
    if (s.length() >= n) {
      return s.substring(0, n);
    }
    return s + " ".repeat(n - s.length());
  }

  private static String padLeft(String s, int n) {
    if (s.length() >= n) {
      return s;
    }
    return " ".repeat(n - s.length()) + s;
  }
}
