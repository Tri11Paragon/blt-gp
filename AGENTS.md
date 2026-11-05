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

- `blt/gp/tree.h`: linear expression trees (`tree_t`), subtree spans and navigation,
  and tree builders (`tree_builder_t`, `generative_tree_builder_t`).
- `blt/gp/functional.h`: function resolver (`function_resolver_t`), function info
  (`function_info_t`), callable wrappers, and function ID management.
- `blt/gp/typing.h`: type ID and metadata registry (`type_resolver_t`).
- `blt/gp/stack.h`: aligned byte storage (`byte_stack_t`) and typed stack utilities.
- `blt/gp/alloc.h`: allocator support.
- `blt/gp/util/traits.h`: callable traits and template utilities.
- BLT utility headers: containers, logging, assertions, formatting, and types.

The build system, test targets, example locations, and dependency setup must be
verified from repository files. Do not assume commands or target names.

## Architecture

### Types and registered functions

- Core APIs live in `blt::gp`; implementation helpers use `blt::gp::detail`.
- Expression trees are homogeneous: `tree_t<Type, Alloc>` and `function_resolver_t<Type, Alloc>`
  are templated on the evaluation value type `Type`.
- A function resolver records callable signatures, dispatch wrappers (`function_wrapper_t`),
  and whether a function produces stored ephemeral values.
- `function_info_t` stores the operator `function_id_t`, its `argument_count` (arity),
  and whether it stores values in the ephemeral stack (`stores_in_stack`).
- Terminals are functions with zero arguments (`argument_count == 0`).
- IDs are meaningful within their originating registry. Do not assume they are
  stable serialized identifiers or interchangeable between resolvers.
- Callable wrappers retain non-owning pointers to registered callable objects.
  Those objects must remain alive and at stable addresses while used.

### Tree representation and evaluation

- Trees use a contiguous sequence of operator metadata (`std::vector<function_info_t>`),
  not individually allocated pointer-linked nodes.
- Operators are stored in evaluation/postorder: children precede their parent,
  and a subtree's root is its final operator.
- Subtrees occupy contiguous half-open operator ranges `[begin, end)`.
- Evaluation walks operators in storage order using a typed evaluation stack
  (`std::vector<Type>`) and returns the root `Type` value directly (`tree.evaluate(resolver)`).
- Preserve operand order carefully: callable argument consumption order, builder emission
  order, and subtree child traversal must agree. Do not infer argument order
  from a traversal method's name alone.
- Do not replace the linear representation with pointer-based nodes without
  explicitly discussing the architectural change.

### Ephemeral values

- Ephemeral values are materialized during tree construction and stored in a
  separate typed vector (`std::vector<Type> ephemeral_stack`).
- Evaluation copies stored ephemeral values onto the evaluation stack in storage
  order rather than invoking their generators again.
- Subtree copying (`to_tree()`), replacement (`replace()`), and erasure (`erase()`)
  update both operator storage and the matching ephemeral range.
- Leading and trailing ephemeral counts must be computed when slicing or mutating subtrees.
- Do not accidentally regenerate ephemeral values during copying or evaluation.

### Evaluation stack and dispatch

- Evaluation uses `std::vector<Type, Alloc<Type>>` as an operand stack.
- Callable wrappers consume the top `arity` arguments from the input stack and
  push the resulting `Type` onto the output stack.
- Dispatch supports using the same stack for input and output (`resolver.forward(id, stack, stack)`).
  Preserve this aliasing behavior when modifying evaluation.
- `Type` must be trivially copyable, move-constructible, or copy-constructible.

### Tree construction

- `tree_builder_t<Type, Alloc>` provides callback-based manual construction.
- `generative_tree_builder_t<Type, Alloc>` provides explicit begin/leaf/ephemeral/end
  construction for algorithms.
- Both builders require a reference to `function_resolver_t<Type, Alloc>` and emit
  the same linear representation (`tree_t<Type, Alloc>`).
- Builder checks validate terminal status, arity matching, and pending node completion.
- Preserve the distinction between ordinary terminals and stored ephemeral
  terminals.

### Ownership and invalidation

- Trees hold operator metadata and ephemeral values; resolvers supply dispatch
  behavior and registry metadata.
- Builders and function resolvers keep non-owning resolver pointers.
- Subtree spans (`subtree_span_t`) are non-owning views into a particular tree.
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
- Homogeneous custom types (e.g., structs / POD types) evaluated across trees.
- Terminal-only trees and nested trees.
- Ephemeral generation once at construction, with stable repeated evaluation.
- Subtree extraction, replacement, and erasure, including ephemeral element ranges.
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