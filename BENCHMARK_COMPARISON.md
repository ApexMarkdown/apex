# Markdown Processor Comparison Benchmark

## Available Tools

Found 7 tools:
- apex
- cmark-gfm
- cmark
- pandoc
- multimarkdown
- kramdown
- marked

## Processor Comparison

**File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/comprehensive_test.md` (17008 bytes, 619 lines)

| Processor | Time (ms) | Relative |
|-----------|-----------|----------|
| apex | 14.60 | 1.00x |
| cmark-gfm | 2.89 | .19x |
| cmark | 2.99 | .20x |
| pandoc | 93.12 | 6.37x |
| multimarkdown | 2.95 | .20x |
| kramdown | 343.38 | 23.52x |
| marked | 87.67 | 6.00x |

## Processor Comparison

**File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/large_doc.md` (29275 bytes, 1094 lines)

| Processor | Time (ms) | Relative |
|-----------|-----------|----------|
| apex | 8.76 | 1.00x |
| cmark-gfm | 2.75 | .31x |
| cmark | 2.53 | .28x |
| pandoc | 111.09 | 12.67x |
| multimarkdown | 2.02 | .23x |
| kramdown | 359.40 | 41.02x |
| marked | 82.27 | 9.38x |

## Apex Mode Comparison

**Test File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/comprehensive_test.md`

| Mode | Time (ms) | Relative |
|------|-----------|----------|
| commonmark | 3.56 | 1.00x |
| gfm | 12.47 | 3.50x |
| mmd | 13.26 | 3.72x |
| kramdown | 12.04 | 3.38x |
| unified | 12.90 | 3.62x |
| default (unified) | 13.34 | 3.74x |

## Apex Feature Overhead

| Features | Time (ms) |
|----------|-----------|
| CommonMark (minimal) | 3.12 |
| + GFM tables/strikethrough | 12.44 |
| + All Apex features | 13.79 |
| + Pretty printing | 14.26 |
| + Standalone document | 13.16 |

---

*Benchmark Complete*
