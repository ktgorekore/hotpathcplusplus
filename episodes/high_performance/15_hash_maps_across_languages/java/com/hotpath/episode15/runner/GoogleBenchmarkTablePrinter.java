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

/** Formats JMH results into a Google Benchmark-style console table. */
public final class GoogleBenchmarkTablePrinter {

  private static final Logger tableLogger =
      LogManager.getLogger("com.hotpath.episode15.runner.TableLogger");

  private static final String RESET = "\u001B[0m";
  private static final String BOLD = "\u001B[1m";
  private static final String GREEN = "\u001B[32m";
  private static final String WHITE = "\u001B[97m";
  private static final String GRAY = "\u001B[90m";

  private final boolean enableColors;

  /**
   * Creates a table printer with optional ANSI terminal color formatting.
   *
   * @param enableColors {@code true} to emit ANSI color escape codes
   */
  public GoogleBenchmarkTablePrinter(boolean enableColors) {
    this.enableColors = enableColors;
  }

  /**
   * Logs JVM/hardware system context followed by a formatted table of JMH run results.
   *
   * @param results collection of completed JMH {@link RunResult} entries
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
    String gcNames =
        gcBeans.stream().map(GarbageCollectorMXBean::getName).collect(Collectors.joining(", "));
    long initHeapMb = memoryBean.getHeapMemoryUsage().getInit() / (1024 * 1024);
    long maxHeapMb = memoryBean.getHeapMemoryUsage().getMax() / (1024 * 1024);

    tableLogger.info("");
    tableLogger.info(timestamp);
    tableLogger.info("Running Episode 15 JMH Benchmarks with Google Benchmark layout");
    tableLogger.info(
        "Run on ({} X CPU threads, {} {})",
        osBean.getAvailableProcessors(),
        osBean.getName(),
        osBean.getArch());
    tableLogger.info("JVM: {} (build {})", runtimeBean.getVmName(), runtimeBean.getVmVersion());
    tableLogger.info("Garbage Collectors: [{}]", gcNames.isEmpty() ? "Default" : gcNames);
    tableLogger.info("Heap Memory: {} MB initial, {} MB maximum", initHeapMb, maxHeapMb);
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
            + padLeft("Time", colTime)
            + " "
            + padLeft("CPU", colCpu)
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
      String row =
          padRight(benchmarkName, colBenchmark)
              + " "
              + padLeft(formatDuration(nsPerOp), colTime)
              + " "
              + padLeft(formatDuration(nsPerOp), colCpu)
              + " "
              + padLeft(formatThroughput(score, primary.getScoreUnit()), colThroughput)
              + " "
              + padLeft(String.valueOf(sampleCount), colIters)
              + " "
              + padLeft(formatError(score, scoreError), colError);
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
      case "ops/s", "thrpt" -> 1_000_000_000.0 / score;
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
    }
    return String.format("%.2f s", ns / 1_000_000_000.0);
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
    }
    return String.format("%.2f ops/s", score);
  }

  private String formatError(double score, double error) {
    if (Double.isNaN(error) || error == 0.0 || score <= 0.0) {
      return "± 0.00%";
    }
    return String.format("± %.2f%%", (error / score) * 100.0);
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
    return s.length() >= n ? s.substring(0, n) : s + " ".repeat(n - s.length());
  }

  private static String padLeft(String s, int n) {
    return s.length() >= n ? s : " ".repeat(n - s.length()) + s;
  }
}
