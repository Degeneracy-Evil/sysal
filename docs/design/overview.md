# Overview

## Project Positioning

**sysal** is a C++ library for collecting and representing server system information.

sysal is **not** a standalone executable by default.
sysal is **not** a benchmark framework.
sysal is **not** an operator scheduler.
sysal is **not** a performance prediction system.

Its responsibility is:

```txt
Collect real system information.
Normalize it into typed data structures.
Return it to the caller through a simple API.
```

sysal should return **facts**, not **decisions**.

For example, sysal may return:

```txt
GPU0 is close to NUMA node 0.
NIC mlx5_0 is associated with PCI address 0000:ca:00.0.
CUDA driver version is available.
The current process can only see GPU0 and GPU1.
```

sysal should not return:

```txt
GPU0 is the best device for GEMM.
NIC mlx5_0 should be selected for communication.
This system has good performance.
```

Those decisions belong to higher-level projects such as opal or opbl.

## Core Design Principle

sysal follows this internal pipeline:

```txt
Read / Probe
    ↓
RawStore
    ↓
Parser
    ↓
ParsedFacts          (internal, per-domain structured facts)
    ↓
Resolver             (cross-reference + conflict resolution + visibility)
    ↓
SystemSnapshot
```

The core principle is:

```txt
Raw evidence first.
Typed model second.
Decision never.
```

All collected information should first enter the raw layer.
The typed system model should be derived from raw evidence.

This design improves:

* debuggability
* testability
* reproducibility
* raw data access
* backend extensibility
* future platform support

## Relationship With Other Projects

```txt
sysal:  What does this system have? What can the current process see?
opal:   Which operator should be selected?
opbl:   How fast does an operator run?
```

sysal remains independent from opal and opbl.
sysal may be used by opal, but contains no opal-specific scheduling logic.

## Architecture Summary

```txt
sysal::collect(spec)
    ↓
Readers collect raw evidence
    ↓
RawStore records original data
    ↓
Parsers extract ParsedFacts (per-domain, no cross-references)
    ↓
Resolver merges facts, resolves conflicts, builds topology / visibility
    ↓
SystemSnapshot returned to caller
```

Core design principle:

```txt
Typed system information library.
Raw evidence based.
Backend independent.
LSP friendly.
No scheduling decisions.
```
