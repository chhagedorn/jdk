---
title: "Assertion Predicates (previously known as \"Skeleton Predicates\")"
date: 2023-05-05
---

_Assertion Predicates_ are a specific kind of predicates found in the C2 compiler that accompany _Hoisted Predicates_ 
created during Loop Predication. Their only purpose is to make sure that the C2 IR is in a consistent state. Compared
to Hoisted Predicates, they do not represent a required check that needs to be executed during runtime to ensure 
correctness. They could be removed after the loop optimization phases - hence the name "Assertion Predicates". But if 
Assertion Predicates are not required at runtime, why do we need them anyway? The reason is found in the very nature
of the C2 IR.

There are other kinds of predicates which I will not cover in this blog article but in a future one. Before 
[the overall predicate renaming](https://bugs.openjdk.org/browse/JDK-8305634), Assertion Predicates have been known
as _Skeleton Predicates_.

# Background

## Sea of Nodes
The IR combines data and control flow into a single representation which is known as sea of nodes<sup>[1](#footnote1)
</sup>. This has a couple of implications - one of them is that data and control always need to be in sync.

### Control and Data Are Always in Sync

If at some point a control path is proven to be never taken, we not only need to remove the control nodes but also the
data nodes. The same principle is also true when data dies: The control path that uses the dead data also needs to be
removed. If we fail to do that, the graph becomes inconsistent and might result in a crash (either by hitting an 
assertion failure during compilation or by emitting wrong code).

With that in mind, let us quickly remind us what Loop Predication is doing.

## Loop Predication
The goal of Loop Predication is to hoist certain checks out of a loop. As a result, we only need to execute the check
once instead of during each iteration which has a positive impact on performance.

### Conditions
Loop Predication can only remove checks (i.e. a `IfNode/RangeCheckNode`) out of a loop if they belong to one of the 
following category:
- The check is loop-invariant (e.g. null checks of objects created outside the loop).
- The check is a range check (i.e. either a `RangeCheckNode` or an `IfNode`<sup>[2](#footnote2)</sup>) of the
  form `i*scale + offset <u array_length`, where `i` is the induction variable, `scale` and `offset` are 
  loop-invariant, `array_length` is the length of an array (i.e.  a `LoadRangeNode`) and the loop is a _counted_ 
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

Range checks are a little bit trickier as we need to cover the index check `i*scale + offset <u array_length` for 
all possible values of the induction variable `i`. Since we have a counted loop and `scale` and `offset` are 
loop-invariant, we actually only need perform the index check for the initial value of `i` (i.e. `init`) and the 
value of `i` in the last iteration (i.e. `last`), assuming a stride `stride` and a loop-limit `limit` (i.e. the loop 
looks like `for (int i = init; i < limit; i += stride`)):

- `init*scale + offset <u array_length` 
- `last*scale + offset = (limit-stride)*scale + offset <u array_length` 

#### Two Hoisted Predicates
We therefore create two Hoisted Predicates for a range check before the counted loop: One with inserting the initial 
value of the induction variable and one with inserting the value of the induction variable in the last iteration 
for the index check `i*scale + offset <u array_length`. The old range check inside the loop is killed.

### Hoisted Predicate Fails at Runtime
If a Hoisted Predicate fail at runtime, we simply jump back to the interpreter and continue execution right before the 
loop. Since Hoisted Predicates are evaluated once before the start of the loop, we have not executed any iteration, yet.
It is therefore safe to resume execution in this way.

Now that we've recapped on how Loop Predication works, we can have a closer look at some problems with Hoisted 
Predicates.

# A Fundamental Issue with Hoisted Predicates

Regardless on how we further optimize a loop (peeling, unrolling, unswitching etc.), if a Hoisted Predicate created for
the loop fails, we will always trap before executing any iteration and resume in the interpreter just before the start
of the loop. However, when splitting a loop into sub-loops, we could face problems with Hoisted Predicates for range
checks due to the mixture of control and data in the C2 IR.

Let's work through an example to illustrate these problems.

## Initial Setup With a Hoisted Predicate for a Range Check
Let's have a look at the following example, assuming that `limit` is a field from which C2 does not know anything about:
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
by taking the maximum value of `i` that we know from the type of `83 Phi`. We therefore insert an
additional `124 CastII` to store the improved type `[0..1]`.

Loop Predication will create two Hoisted Predicates for `111 RangeCheck` and remove this range check from the loop. We 
will denote the Hoisted Predicates with `PU(a[i])` for the upper bound and `PL(a[i])` for the lower bound:
```java
// Check: init*scale + offset <u array_length` <=> 1*1 + 0 <u a.length <=> 
// 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length <=> (limit-1)*1 + 0 <u a.length <=> 
// limit-1 <u a.length
PL(a[i])
for (int i = 1; i > limit; i -= 2) {
    a[i] = 34;
}
```

So far, so good.

## Splitting the Loop into Sub-Loops where one of them is Dead
Now let's assume that we apply _Loop Peeling_ for the loop in our example<sup>[3](#footnote3)</sup>. Loop Peeling 
splits the first iteration from the loop to execute it before the remaining loop. We can do that by cloning the loop 
and changing the bounds of the first loop in such a way that it only runs for one iteration. We are therefore left with 
two sub-loops: The first runs for a single iteration and the second runs the remaining iterations:
```java
// Check: init*scale + offset <u array_length` <=> 1*1 + 0 <u a.length <=> 
// 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length <=> (limit-1)*1 + 0 <u a.length <=> 
// limit-1 <u a.length
PL(a[i])
for (int i = 1; i > -1; i -= 2) {
    a[i] = 34;
}
for (int i = -1; i > -1; i -= 2) {
    a[i] = 34;
}
```
The first loop is not a real loop anymore as we are never taking the backedge. IGVN will therefore fold this single
loop iteration into a simple sequence of statements, achieving the goal of Loop Peeling:

```java
// Check: init*scale + offset <u array_length` <=> 1*1 + 0 <u a.length <=> 
// 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length <=> (limit-1)*1 + 0 <u a.length <=> 
// limit-1 <u a.length
PL(a[i])
a[1] = 34;
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

### Propagating Top to All Uses of the CastII node
IGVN will do its work and propagate `top` to all uses of the `CastII` node - possibly converting these nodes to 
`top` as well. This triggers a cascade of removals which is only stopped at nodes which can handle `top` in such a 
way that the node itself does not die (e.g. a `PhiNode`).

### The Remaining Loop is Dead Anyway - Where is the Problem now?
You might argue now that looking at the code after Loop Peeling that the remaining loop starting at `-1` will never 
be executed and is therefore dead anyway. This is true because if `limit < 0`, then the Hoisted Predicate will 
just fail at runtime. If `limit >= 0`, then the loop will not be entered. Either way, the loop is never 
executed and is dead. We could just remove it - but how?

### Won't the Replacement of the `CastII` Node with Top Be Enough?
Unfortunately, it's not. Let us remind us of the fundamental property that data and control always needs to be in 
sync. If data in one path dies, this control path needs to be removed as well - but that is not happening! Even though 
the loop is evidently dead at runtime, C2 does not know that. There is no `IfNode` or anything alike that would take 
care of removing the remaining dead sub-loop after peeling. The graph is left in an inconsistent state: Some 
data/control/memory nodes could have been removed while others still remain inside the dead loop. This _could_ lead 
to assertion failures or even the emission of wrong code.

### CastII Node Updates Are Updated - Hoisted Predicates Are Not

Why does this happen? The problem is that each time we are updating the initial value of the induction variable, the
stride and/or the limit of the loop, the involved
`CastII` node for the array index (and potentially other `CastConstraint` nodes dependent on the induction variable)
are updated while Hoisted Predicates are not. If a Hoisted Predicate cannot be proven to always fail, then we still
cannot prove it after splitting the loop in an arbitrary way (assuming that we will not find out anything new about the
loop in the meantime).

### How Can We Fix This?
How can this discrepancy between `CastConstraint` nodes and Hoisted Predicates be prevented? How can we ensure a 
dead loop is cleaned up when data is dying? How can we establish the synchronization of control and data again? The
answer is Assertion Predicates.

# Assertion Predicates
Now that we understand the fundamental issue with Hoisted Predicates, we need to address this problem. Our solution 
is to set up a special version of each Hoisted Predicate for each sub-loop that reflects the new initial value of the 
induction variable, the new stride, and the new limit. We call these versions _Assertion Predicates_.

## Fold Dead Sub-Loop Away
Having these additional Assertion Predicates in place will now take care to fold dead sub-loops away. Let's revisit 
our example from the last section and see how we can create Assertion Predicates when peeling a loop:
```java
// Check: init*scale + offset <u array_length` <=> 1*1 + 0 <u a.length <=> 
// 1 <u a.length
PU(a[i])
// Check: (limit-stride)*scale + offset <u array_length <=> (limit-1)*1 + 0 <u a.length <=> 
// limit-1 <u a.length
PL(a[i])
for (int i = 1; i > -1; i -= 2) {
    a[i] = 34;
}

// 2 Assertion Predicates - Updated Version of the 2 Hoisted Predicates PU and PL above
// Check: init*scale + offset <u array_length` <=> -1*1 + 0 <u a.length <=> 
// -1 <u a.length
AU(a[i])
// Check: (limit-stride)*scale + offset <u array_length <=> (limit-1)*1 + 0 <u a.length <=> 
// limit-1 <u a.length (Same as PL since we did not update the stride or limit).
AL(a[i])
for (int i = -1; i > -1; i -= 2) {
    a[i] = 34;
}
```
We can now immediately see that `AU` compares `-1 <u a.length` which will always fail. IGVN is therefore able to 
safely remove the created dead sub-loop and its involved data. The graph will be consistent again.

## Always True at Runtime
An Assertion Predicate will always be true during runtime because it re-checks something we've already covered with 
a Hoisted Predicate. We can therefore completely remove an Assertion Predicate once loop opts are over. But for 
debugging purpose, it helps to keep these Assertion Predicates alive to ensure that we never hit them. To do that, 
we implement the failing path with a `HaltNode`. Executing the `HaltNode` at runtime will result in a crash.

## Always Added/Updated When Modifying the Initial Value of an Induction variable, the Stride or the Limit of a Loop
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
Even though Loop Predication has been around for quite a while<sup>[4](#footnote4)</sup>, this problem was only 
detected years later. At that time, the scope of the problem was not yet entirely clear. We've started adding 
Assertion Predicates (back then known as _Skeleton Predicates_) for different loop optimizations as new 
failing testcases emerged. Over the time, more and more `CastConstraint` nodes were added and made the uncommon case 
more likely - especially with more enhanced fuzzer testing.

What started out as a simple, clean fix (relatively speaking) when first introducing Skeleton Predicates in 
[JDK-8193130](https://bugs.openjdk.org/browse/JDK-8193130), became much more complex with each new fix. The latest
bigger fix, at the time of this writing, was to add Assertion Predicates at Loop Peeling. And yet, even though we now
cover all loop optimizations, there are still cases which we do not cover when combining different loop optimizations
together. The code became very complex and difficult to maintain over the years and fixing these cases nearly
impossible. 

## Cleaning up Predicates and Fixing Assertion Predicates
On top of these problems with Assertion Predicates, we have other kinds of predicates in the C2 code which we did not 
properly name. This made talking about "predicates" even more confusing as we've lacked a clear naming scheme. All of 
that led us to come to the conclusion that it's time to bring order into the C2 predicates. We wanted:
- A clear naming scheme of predicates
- Cleaning up predicate code (i.e. variable and method names, predicate matching code, comments, clearly defined 
  methods to skip over predicates etc.)
- Redesigning Assertion Predicates to fix the remaining issues

The first two goals are prerequisites to make the redesign of Assertion Predicates simpler and even possible. 

# Conclusion
We've discussed a long-standing issues with Hoisted Predicates and how this was attempted to be fixed over the last 
years with Assertion Predicates. A complete solution is yet to be added. However, we've filed [JDK-8288981](
https://bugs.openjdk.org/browse/JDK-8288981) with its sub-tasks to achieve this. Over the last months, I've shifted my 
focus towards achieving this goal. The cleanup and re-implementation of Assertion Predicates is almost done as of 
today. The only thing left is to clean up the code further and heavily test my patches to fix some hidden bugs.

I've written this blog post mainly to give an introduction into Assertion Predicates but also to ease the review 
process of my patches - at least on a high level. A follow-up blog post should cover the remaining predicates and 
how they are used within C2.

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

<a name="footnote4"><sup>4</sup></a>First version added with [JDK-6894778](https://bugs.openjdk.org/browse/JDK-6894778).

<a name="footnote5"><sup>5</sup></a>See [JDK-8283466](https://bugs.openjdk.org/browse/JDK-8283466).

---

**Disclaimer**: I provide this information as a guidance and learning opportunity - for you and me. Use this 
information with care as it could potentially be subject to change or contain errors (which I do not hope).

