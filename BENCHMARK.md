# Apex Markdown Processor - Performance Benchmark

## Test Document

- **File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/comprehensive_test.md`
- **Lines:**      619
- **Words:**     2581
- **Size:**    17008 bytes

## Output Modes

| Mode | Iterations | Average (ms) | Min (ms) | Max (ms) | Throughput (words/sec) |
|------|------------|--------------|---------|---------|------------------------|
| Fragment Mode (default HTML output) | 50 | 22 | 20 | 36 | 129050.00 |
| Pretty-Print Mode (formatted HTML) | 50 | 22 | 21 | 27 | 129050.00 |
| Standalone Mode (complete HTML document) | 50 | 21 | 20 | 29 | 129050.00 |
| Standalone + Pretty (full features) | 50 | 22 | 21 | 29 | 129050.00 |

## Mode Comparison

| Mode | Iterations | Average (ms) | Min (ms) | Max (ms) | Throughput (words/sec) |
|------|------------|--------------|---------|---------|------------------------|
| CommonMark Mode (minimal, spec-compliant) | 50 | 11 | 11 | 14 | 258100.00 |
| GFM Mode (GitHub Flavored Markdown) | 50 | 27 | 20 | 203 | 129050.00 |
| MultiMarkdown Mode (metadata, footnotes, tables) | 50 | 22 | 20 | 28 | 129050.00 |
| Kramdown Mode (attributes, definition lists) | 50 | 20 | 20 | 23 | 129050.00 |
| Unified Mode (all features enabled) | 50 | 22 | 20 | 33 | 129050.00 |
| Default Mode (unified, all features) | 50 | 21 | 20 | 25 | 129050.00 |

---

*Benchmark Complete*
