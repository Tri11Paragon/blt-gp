# Agent guide: blt-gp

## Purpose and scope

blt-gp is a C++17 project with typed expression-tree infrastructure for
genetic programming.

This file captures durable architectural context and constraints for coding
agents. Treat source code and tests as the authority for current behavior.
Do not treat planned features or incomplete validation as implemented guarantees.

Before changing a subsystem, inspect its implementation, callers, and tests.
Update this guide when an intentional architectural decision changes.

## Repository orientation

Locate the actual paths in the checkout before making edits. The core components
visible at the time this guide was written are:

- `blt/gp/tree.h`: type and function registration, callable dispatch, linear expression
  trees, subtree operations, and tree builders.
- `blt/gp/stack.h`: aligned byte storage and typed stack argument access.
- `blt/gp/alloc.h`: allocator support.
- `blt/gp/util/traits.h`: callable traits and template utilities.
- BLT utility headers: containers, logging, assertions, formatting, and types.

The build system, test targets, example locations, and dependency setup must be
verified from repository files. Do not assume commands or target names.

## Architecture

### Types and registered functions

- Core APIs live in `blt::gp`; implementation helpers use `blt::gp::detail`.
- A type resolver assigns local type IDs and records size, aligned size, and
  display names.
- A function resolver records callable signatures, dispatch wrappers, optional
  backward callbacks, and whether a function produces stored ephemeral values.
- Terminals are functions with zero arguments.
- IDs are meaningful within their originating registry. Do not assume they are
  stable serialized identifiers or interchangeable between resolvers.
- Callable wrappers retain non-owning pointers to registered callable objects.
  Those objects must remain alive and at stable addresses while used.

### Tree representation and evaluation

- Trees use a contiguous sequence of operator metadata, not individually
  allocated pointer-linked nodes.
- Operators are stored in evaluation/postorder: children precede their parent,
  and a subtree's root is its final operator.
- Subtrees occupy contiguous half-open operator ranges `[begin, end)`.
- Evaluation walks operators in storage order using a byte stack.
- Preserve operand order carefully: callable argument offsets, builder emission
  order, and subtree child traversal must agree. Do not infer argument order
  from a traversal method's name alone.
- Do not replace the linear representation with pointer-based nodes without
  explicitly discussing the architectural change.

### Ephemeral values

- Ephemeral values are materialized during tree construction and stored in a
  separate byte stack.
- Evaluation copies their stored bytes rather than invoking their generators
  again.
- Their byte layout follows the corresponding operators' storage order and
  aligned return sizes.
- Subtree copying, replacement, and erasure must update both operator storage
  and the matching ephemeral byte range.
- Do not accidentally regenerate ephemeral values during copying or evaluation.

### Byte-stack contract

- Stack storage uses a custom allocator and fixed 8-byte slot alignment.
- Typed argument offsets and byte counts depend on that shared alignment rule.
  Any layout change must update all producers and consumers together.
- Do not assume support for types requiring alignment greater than 8 bytes.
- Storage operations include raw byte copies and do not provide general
  non-trivial object lifetime management.
- Callable return values are currently constrained to trivially copyable types.
  Do not generalize this into unrestricted support for arbitrary C++ objects.
- References and pointers into stack storage can be invalidated by resizing or
  reallocation.
- Dispatch supports using the same stack for input and output. Preserve this
  aliasing behavior when modifying evaluation.

### Tree construction

- `tree_builder_t` provides callback-based manual construction.
- `generative_tree_builder_t` provides explicit begin/leaf/ephemeral/end
  construction for algorithms.
- Both emit the same linear representation.
- Builder checks differ. Do not assume complete type compatibility validation
  merely because arity or construction-state checks exist.
- Preserve the distinction between ordinary terminals and stored ephemeral
  terminals.

### Ownership and invalidation

- Trees hold operator metadata and ephemeral bytes; resolvers supply dispatch
  behavior and registry metadata.
- Builders and function resolvers keep non-owning resolver pointers.
- Subtree spans are non-owning views into a particular tree.
- Structural edits can invalidate spans and operator indices. Reacquire views
  after mutation rather than relying on stale indices.
- Do not assume thread safety or portable serialization unless explicitly
  established by the relevant implementation.

## Implementation conventions

- Target C++17. Do not introduce newer language or library requirements without
  agreement.
- Follow neighboring formatting: four-space indentation, Allman-style braces,
  and existing naming conventions such as `_t` type suffixes.
- Preserve existing license and copyright headers.
- Preserve configurable vector/container templates where already supported.
- Prefer existing BLT utilities when consistent with nearby code.
- Keep changes focused. Avoid unrelated formatting, API churn, new dependencies,
  or architectural rewrites.
- Separate observed behavior from intended behavior when documenting incomplete
  or suspicious code.

## Validation expectations

Discover and use the repository's existing build and test workflow. Report
exactly what was run and any checks that could not be run.

For changes affecting the core representation, cover the relevant cases:

- Non-commutative multi-argument functions to detect operand-order regressions.
- Mixed argument sizes and alignment boundaries.
- Terminal-only trees and nested trees.
- Ephemeral generation once at construction, with stable repeated evaluation.
- Subtree extraction, replacement, and erasure, including ephemeral byte ranges.
- Invalid builder sequences, arity mismatches, and multiple-root attempts.
- Same-stack input/output dispatch.
- Callable lifetime and storage invalidation where relevant.

Use existing sanitizer or benchmark configurations when available for memory
layout or performance-sensitive changes. Do not claim validation that was not
performed.

## Maintaining persistent context

Keep this file concise and durable. Do not add chat transcripts, temporary task
status, or unverified assumptions.

For a substantial design decision, use the repository's existing decision-record
convention. If none exists, consider a short record under `docs/decisions/`
containing the decision, rationale, alternatives, and consequences; link it here.

When behavior and this guide diverge, investigate whether the code changed
intentionally before updating either.