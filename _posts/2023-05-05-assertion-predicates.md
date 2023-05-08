---
title: "Assertion Predicates (previously known as \"Skeleton Predicates\")"
date: 2023-05-05
---

_Assertion Predicates_ are a specific kind of predicates found in the C2 compiler that accompany _Hoisted Predicates_ 
created during _Loop Predication_. Their only purpose is to make sure that the C2 IR is in a consistent state. Compared
to Hoisted Predicates, they do not represent a required check that needs to be executed during runtime to ensure 
correctness. They could be removed after the loop optimization phases - hence the name "Assertion Predicates". But if 
Assertion Predicates are not required at runtime, why do we need them anyway? The reason is found in the very nature
of the C2 IR.

There are other kinds of predicate which I will not cover in this blog article but possibly in a future one. 
Important to note here is that before [the overall predicate renaming](https://bugs.openjdk.org/browse/JDK-8305634), 
Assertion Predicates have been known as _Skeleton Predicates_.

# Background

## Sea of Nodes

The IR combines data and control flow into a single representation which is known as sea of nodes<sup>[1](#footnote1)
</sup>. This has a couple of implications - one of them is that data and control always need to be in sync.

### Control and Data Are Always in Sync

If at some point a control path is proven to be never taken, we not only need to remove the control nodes but also the
data nodes specific to that path. The same principle is also true when data nodes die: All control nodes on the path
that use the dead data nodes also need to be removed. While the first property is not that difficult to maintain
(data nodes without uses are just removed), the latter is not that straight forward. Sure, control nodes without uses
are also just removed, but we need to be more careful. We are required to _completely_ remove all control nodes leading
up to the dead data node uses (i.e. all control paths). These could be inside a nested loop which we need to kill
entirely. Failing to do so, could leave the graph in an inconsistent state. This will have unpredictable consequences in
further optimizations and code emission. We could end up hitting an assertion failure further down the road or emitting 
wrong code, resulting in a crash or wrong execution at runtime.

With that in mind, let us quickly remind us what Loop Predication is doing.

## Loop Predication

The goal of Loop Predication is to hoist certain checks out of a loop. As a result, we only need to execute these checks
once before the start of the loop instead of during each iteration which has a positive impact on performance.

### Conditions

Loop Predication can only remove checks (i.e. a `IfNode/RangeCheckNode`) out of a loop if they belong to one of the 
following category:
- The check is loop-invariant (e.g. null checks of objects created outside the loop).
- The check is a range check (i.e. either a `RangeCheckNode` or an `IfNode`<sup>[2](#footnote2)</sup>) of the
  form `i*scale + offset <u array_length`, where `i` is the induction variable, `scale` and `offset` are 
  loop-invariant, `array_length` is the length of an array (i.e.  a `LoadRangeNode`), and the loop is a _counted_ 
  loop (i.e. represented by a `CountedLoopNode` inside the IR).

#### Why Do Array Range Checks Have a Single Unsigned Comparison?

For an array, we need to check that the index is zero or positive _and_ strictly less than the size of the array:
- `i*scale + offset >= 0`
- `i*scale + offset < array_length` 

This means that we would need two checks for each array access. But we can combine them into a single check by using 
the following unsigned comparison trick (converting `i*scale + offset` and `array_length` to unsigned and do the 
comparison):
```
i*scale + offset <u array_length
```
Let's see what happens when we convert `i*scale + offset` to unsigned:
- If `i*scale + offset < 0`: Converting `i+scale + offset` to unsigned results in an integer greater than `MAX_INT`
  and therefore greater than `array_length` which is at most `MAX_INT`: 
  ```
  MAX_UINT >= (unsigned)(i*scale + offset) > MAX_INT >=  array_length
  ```
  The unsigned comparison will therefore fail.

- If `i*scale + offset >= array_size`: The check just fails as it would have if it had been a signed comparison.

### Hoisting Invariant Checks

Once we find a check that qualifies to be hoisted out of a loop, we move it right before the loop. This is straight
forward for invariant checks, where we can simply re-use the condition inside the loop as a condition for the check 
outside the loop.

#### One Hoisted Predicate

We therefore create a single Hoisted Predicate for a loop-invariant check before the loop and kill the old check inside 
the loop.

### Hoisting Range Checks

Range checks are a little bit trickier as we need to cover the index check `i*scale + offset <u array_length` for all
possible values of the induction variable `i`. Since `scale` and `offset` are loop-invariant, and we have a counted
loop, we actually only need to perform the index check for the initial value of `i` (i.e. `init`) and the value of
`i` in the last iteration (i.e. `last`), assuming a stride `stride` and a loop limit `limit` (i.e. the loop looks like
`for (int i = init; i < limit; i += stride`)):

- `init*scale + offset <u array_length` 
- `last*scale + offset = (limit-stride)*scale + offset <u array_length` 

#### Two Hoisted Predicates

We therefore create two Hoisted Predicates for a range check before the counted loop: One with inserting the initial 
value of the induction variable and one with inserting the value of the induction variable in the last iteration 
for the index check `i*scale + offset <u array_length`. The old range check inside the loop is killed.

### What Happens if a Hoisted Predicate Fails at Runtime

If a Hoisted Predicate fails at runtime, we trap and simply jump back to the interpreter and continue execution right
before the loop. Since Hoisted Predicates are evaluated once before the start of the loop, we have not executed any
iteration of the loop, yet. It is therefore safe to resume execution in this way.

Now that we've recapped on how Loop Predication works, we can have a closer look at why this simple idea of creating 
Hoisted Predicates introduces a new fundamental problem for the C2 IR. 

# A Fundamental Issue with Hoisted Predicates

Regardless on how we further optimize a loop (peeling, unrolling, unswitching etc.), if a Hoisted Predicate for a loop
fails, we will always trap before executing any iteration and resume in the interpreter just before the start of the
loop. However, when splitting such a loop into sub-loops, we could run into a fundamental problem for Hoisted 
Predicates created for range checks due to the mixture of control and data in the C2 IR.

Let's work through an example to illustrate why.

## Loop Predication Example to Hoist a Range Check

Let's have a look at the following example, assuming that `limit` is a field from which C2 does not know anything 
about (i.e. its type is just `#int`):
```java
for (int i = 1; i > limit; i -= 2) {
    a[i] = 34;
}
```
C2 emits a `RangeCheckNode` for the array access `a[i]`:

![RangeCheckNode inside loop](/jdk/assets/img/2023-05-05-assertion-predicates/range_check.png)

`83 Phi` is the induction variable `i` and `108 LoadRange` the size of the array. C2 knows that the type of `83 Phi`
is `[min+1..1]` because we start at the initial value `i = 1`, the stride is `-2`, and `limit` could be anything (i.e.
worst case `MIN_INT` and thus `i` cannot get smaller than `MIN_INT + 1`). We know, by design, that after performing any
range check, an array index `i` will always satisfy `0 <= i < MAX_INT`. On top of that, we can improve the upper bound
of the index by taking the maximum value of `i` that we know from the type of `83 Phi`. We therefore insert an
additional `124 CastII` to store the improved type `[0..1]` for the array index at `a[i]`.

Loop Predication will identify `111 RangeCheck` as valid range check and removes it from the loop by creatnig two
Hoisted Predicates. We will denote the Hoisted Predicates with `PU(a[i])`, for the upper bound, and `PL(a[i])`, for the
lower bound:
```java
// Check: init*scale + offset <u array_length`
// <=> 1*1 + 0 <u a.length 
// <=> 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length 
// <=> (limit-1)*1 + 0 <u a.length
// <=> limit-1 <u a.length
PL(a[i])
for (int i = 1; i > limit; i -= 2) {
    a[i] = 34;
}
```

So far, so good.

## Splitting the Loop into Sub-Loops where One of Them Is Dead

Now let's assume that we apply _Loop Peeling_ for the loop in our example<sup>[3](#footnote3)</sup>. Loop Peeling splits
the first iteration from the loop to execute it before the remainder of the loop. We can do that by cloning the loop and
changing the bounds in such a way that it only runs for one iteration. We are therefore left with two sub-loops: The
first runs for a single iteration and the second runs the remaining iterations:
```java
// Check: init*scale + offset <u array_length`
// <=> 1*1 + 0 <u a.length 
// <=> 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length 
// <=> (limit-1)*1 + 0 <u a.length
// <=> li
PL(a[i])

// Peeled iteration
for (int i = 1; i > -1; i -= 2) {
    a[i] = 34;
}

// Remaining loop
for (int i = -1; i > limit; i -= 2) {
    a[i] = 34;
}

```
The first loop is not a real loop anymore as we are never taking the backedge. IGVN will therefore fold this single
loop iteration into a simple sequence of statements, achieving the goal of Loop Peeling:

```java
// Check: init*scale + offset <u array_length`
// <=> 1*1 + 0 <u a.length 
// <=> 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length 
// <=> (limit-1)*1 + 0 <u a.length
// <=> li
PL(a[i])

// Peeled iteration
a[1] = 34;

// Remaining loop
for (int i = -1; i > limit; i -= 2) {
    // Type of iv phi i [min+1..-1]
    a[CastII(i)] = 34;
}
```

IGVN will also update types, including the types of the induction variable `83 Phi` and array index `124 CastII`.
`83 Phi` will use the new initial value of the induction variable, `-1`, and update its type to `[min+1..-1]`
accordingly. This type is propagated to its output `124 CastII` which computes its own new type (see `CastII::Value()`)
by taking this input type and joining it with its already stored type `[0.. 1]`. But these two type ranges do not
overlap! In such a case, we set `top` as type - meaning an empty set of possible values. IGVN will then replace the 
entire `CastII` node with the dedicated `top` node.

### Propagating Top to All Uses of the CastII Node

IGVN will do its work and propagate `top` to all uses of the `CastII` node - possibly converting these nodes to 
`top` as well. This triggers a cascade of removals which is only stopped at nodes which can handle `top` in such a 
way that the node itself does not die (e.g. a `PhiNode`).

### The Remaining Loop Is Dead Anyway - Where Is the Problem Now?

You might argue now while looking at the code after Loop Peeling that the remaining loop, starting at `-1`, will never
be executed. This is true because if `limit < 0`, then the Hoisted Predicate will fail at runtime and we trap. If 
`limit >= 0`, then the loop will not be entered. Either way, the loop is never executed and evidently dead. So why 
don't we just remove it and go home? Well, yes, but how?

### Won't the Replacement of the CastII Node with Top Kill the Dead Sub-Loop?

Unfortunately, no, it will not. Let us remind us of the fundamental IR graph property that data and control always need
to be in sync. If data in one path dies, this control path needs to be removed as well - but that is not automatically
happening! Even though the loop is evidently dead at runtime, C2 does not know that. There is no `IfNode` or anything
alike that would take care of removing the remaining dead sub-loop after peeling. The graph is left in an inconsistent
state: Some data/control/memory nodes could have been removed while others still remain inside the dead loop. This
_could_ lead to assertion failures even though any of the not yet cleared nodes inside this dead sub-loop will never be 
executed at runtime.

### CastII Nodes Are Updated - Hoisted Predicates Are Not

How did we end up in this situation? The problem is that each time we are updating the initial value of the induction
variable, the stride and/or the limit of the loop, the involved `CastII` node for the array index (and potentially
other `CastConstraint` nodes dependent on the induction variable, stride and/or limit) are updated while Hoisted
Predicates are not. If a Hoisted Predicate cannot be proven to always fail, then we still cannot prove it after
splitting the loop in an arbitrary way (assuming that we will not find out anything new about the loop in the meantime).

### How Can We Fix This?
How can this discrepancy between `CastConstraint` nodes and Hoisted Predicates be prevented? How can we ensure a 
dead sub-loop is cleaned up when data is dying inside it? How can we establish the synchronization of control and data 
again? The answer lies within Assertion Predicates.

# Assertion Predicates

Now that we understand the fundamental issue with Hoisted Predicates, we need to address it. Our solution is to set up a
special version of each Hoisted Predicate for each sub-loop such that it reflects any new initial value of the induction
variable, new stride, and new limit. We call these versions _Assertion Predicates_.

## Fold Dead Sub-Loop Away

Having these additional Assertion Predicates in place will now take care of folding dead sub-loops away. Let's revisit 
our example from the last section and see how we can create Assertion Predicates `AU` and `AL` as special versions 
of the Hoisted Predicates `PU` and `PL` when peeling a loop:
```java
// Check: init*scale + offset <u array_length`
// <=> 1*1 + 0 <u a.length 
// <=> 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length 
// <=> (limit-1)*1 + 0 <u a.length
// <=> li
PL(a[i])

// Peeled iteration
a[1] = 34;

// Remaining loop 

// 2 Assertion Predicates - updated version of PU and PL above
// Check: init*scale + offset <u array_length` 
// <=> -1*1 + 0 <u a.length
// <=> -1 <u a.length
AU(a[i])
// Check: (limit-stride)*scale + offset <u array_length 
// <=> (limit-1)*1 + 0 <u a.length 
// <=> limit-1 <u a.length (Same as PL because stride and limit are the same)
AL(a[i])
for (int i = -1; i > -1; i -= 2) {
    a[i] = 34;
}
```

We can immediately see that `AU` compares `-1 <u a.length` which, obviously, always fails. We would therefore never 
enter the loop. Accordingly, IGVN is now able to safely remove the created dead sub-loop and its involved data. The 
graph is consistent again.<sup>[4](#footnote4)</sup>

## Always True at Runtime

An Assertion Predicate will always be true during runtime because it re-checks something we've already covered with 
a Hoisted Predicate. We can therefore completely remove an Assertion Predicate once loop optimizations are over. But for
debugging purpose, it helps to keep these Assertion Predicates alive to ensure that none of these will ever fail. To do 
that, we implement the failing path with a `HaltNode` instead of a trap. Executing the `HaltNode` at runtime will 
result in a crash.

## Always Added/Updated when Modifying the Initial Value of an Induction Variable, the Stride or the Limit of a Loop

We always insert Assertion Predicates when changing a loop, for which we created Hoisted Predicates, in such a way 
that either the initial value of the induction variable, the stride, or the limit is updated. This happens when:
- Peeling a Loop: 
  - The remaining loop needs Assertion Predicates.
- Creating Pre/Main/Post Loops: 
  - The main and post loop need Assertion Predicates.

Furthermore, we need to update existing Assertion Predicates when:
- Unrolling a Main Loop:
  - The main loop Assertion Predicates need to be updated because we change the stride.
- Unswitching a Loop:
  - The fast and the slow loop need a version of the Assertion Predicate (we could split these loops further).

## Implementation Attempts
Even though Loop Predication has been around for quite a while<sup>[5](#footnote5)</sup>, this problem was only 
detected years later. At that time, the scope of the problem was not yet entirely clear. We've started adding 
Assertion Predicates (back then known as _Skeleton Predicates_) for different loop optimizations as new 
failing testcases emerged. Over the time, more and more `CastConstraint` nodes were added which made the still 
very uncommon case more likely - especially with more enhanced fuzzer testing.

What had started out as a simple, clean fix (relatively speaking), when Skeleton Predicates were first introduced in
[JDK-8193130](https://bugs.openjdk.org/browse/JDK-8193130), became much more complex with each new fix. The latest
bigger fix, at the time of this writing, was to add Assertion Predicates at Loop Peeling<sup>[6](#footnote6)</sup>. And
yet, even though we now cover all loop optimizations, there are still cases which we do not cover properly when 
combining different loop optimizations together. The code became very complex and difficult to maintain over the years
and fixing these cases nearly impossible.

## Cleaning up Predicates and Fixing Assertion Predicates

On top of these problems with Assertion Predicates, we have other kinds of predicates in the C2 code which we did not 
properly name. This made talking about "predicates" even more confusing as we've lacked a clear naming scheme. All of 
that led us to come to the conclusion that it's time to bring order into the C2 predicates. We wanted:
- A clear naming scheme of predicates
- Cleaning up the predicate code (i.e. variable and method names, predicate matching code, comments, unifying and 
  grouping methods to skip over predicates together etc.)
- Redesigning Assertion Predicates to fix the remaining issues

The first two goals are prerequisites to make the redesign of Assertion Predicates simpler and only even possible in 
a clean way. 

# Conclusion
We've discussed a long-standing issues with Hoisted Predicates and how this was attempted to be fixed over the last 
years with Assertion Predicates. A complete solution is yet to be added. However, we've filed [JDK-8288981](
https://bugs.openjdk.org/browse/JDK-8288981) with its sub-tasks to achieve this. Over the last months, I've shifted my 
focus towards achieving this goal. The cleanup and re-implementation of Assertion Predicates is almost done as of 
today. The only major thing left is to clean up the code further and heavily test my patches ensure correctness.

I've written this blog post mainly to give an introduction into Assertion Predicates but also to ease the review 
process of my patches - at least on a high level. A potential follow-up blog post should cover the remaining predicates 
and how they are used within C2.

---
<a name="footnote1"><sup>1</sup></a>Also see [A Simple Graph-Based Intermediate Representation](
https://www.oracle.com/technetwork/java/javase/tech/c2-ir95-150110.pdf).

<a name="footnote2"><sup>2</sup></a>Almost always, a range check will be a `RangeCheckNode` as it is unusual to write
code that would emit an `IfNode` with an unsigned comparison with a `LoadRangeNode`, a trap on the uncommon path,
and satisfying all other conditions to qualify for being hoisted as a range check out of a counted loop.

<a name="footnote3"><sup>3</sup></a>Loop Peeling would not actually be triggered for this example as there is no 
reason to peel (see [IdealLoopTree::estimate_peeling()](
https://github.com/chhagedorn/jdk/blob/1be80a4445cf74adc9b2cd5bf262a897f9ede74f/src/hotspot/share/opto/loopTransform.cpp#L452-L504)).
But for the sake of simplicity, let's assume that C2 actually applies Loop Peeling for this example.

<a name="footnote4"><sup>4</sup></a>The careful reader might ask now how we can prevent to accidentally remove the 
entire remaining graph when folding an Assertion Predicate away? C2 actually establishes a dedicated 
_zero-trip guard_ which checks beforehand if the remaining loop should be entered depending on the induction 
variable and the limit. We put the Assertion Predicates right into the "enter the loop"-path of the zero-trip guard 
check. Since we would never enter the dead-sub loop, Assertion Predicates will only kill the dead sub-loop inside the
"enter the loop"-path. Note that a zero-trip guard and an Assertion Predicate are not the same. The former checks the
induction variable against the loop limit while the latter compares it against the array length. Let's see how the IR
actually looks like in our example with the dedicated zero-trip guard after peeling the loop:
```java
// Check: init*scale + offset <u array_length`
// <=> 1*1 + 0 <u a.length 
// <=> 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length 
// <=> (limit-1)*1 + 0 <u a.length
// <=> li
PL(a[i])

// Peeled iteration
a[1] = 34;

// Remaining loop 

// Zero-trip guard
if (i > limit) {
  // 2 Assertion Predicates - updated version of PU and PL above
  // Check: init*scale + offset <u array_length` 
  // <=> -1*1 + 0 <u a.length
  // <=> -1 <u a.length
  AU(a[i])
  // Check: (limit-stride)*scale + offset <u array_length 
  // <=> (limit-1)*1 + 0 <u a.length 
  // <=> limit-1 <u a.length (Same as PL because stride and limit are the same)
  AL(a[i])
  // The IR uses a do-while structure
  do {
      a[i] = 34;
      i -= 2;
  while (i > -1);
}
```

<a name="footnote5"><sup>5</sup></a>First version added with [JDK-6894778](https://bugs.openjdk.org/browse/JDK-6894778).

<a name="footnote6"><sup>6</sup></a>See [JDK-8283466](https://bugs.openjdk.org/browse/JDK-8283466).

---

**Disclaimer**: All the information provided in my blog posts reflects my point of view - use everything with care. 
My goal is to give easily understandable introductions, overviews, guidances, and/or learning opportunities - for 
you and me. To achieve that, I aim for simplicity and focus on ideas, rather than getting lost in details to get 
absolute completeness. This means that I sometimes omit implementation specific details to keep things simple 
and more general. The truth will always be found in the actual code. Keep in mind that, generally, anything mentioned
in my blog posts could potentially be subject to change at any point.

