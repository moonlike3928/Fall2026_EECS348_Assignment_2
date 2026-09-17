# EECS 348 — Assignment 2: GenAI Code Comparison Analysis

**Author:** David Joslin
**KUID:** 3218103
**Lab:** lab2
**Date:** 09/17/2026

## 1. Names of the Two GenAI Programs Used

- **GenAI #1:** Claude Opus 5 (model id `claude-opus-5`), Anthropic
- **GenAI #2:** Claude Haiku 4.5 (model id `claude-haiku-4-5-20251001`), Anthropic

**Note on tool choice:** Rather than ChatGPT/Copilot, I used two different Claude model tiers as the two independent "GenAI" code-generation sources run as separate, isolated subagents inside Claude Code with no access to each other's output. 

## 2. How Each GenAI Was Accessed

Both models were accessed as autonomous coding subagents launched from Claude Code (Anthropic's CLI coding agent) running on the author's own machine, each given the same task prompt in an isolated context with no visibility into the other agent's work or output. Each agent independently wrote its C file to disk, compiled it with `gcc`, and ran it against the sample test before reporting back.

## 3. Prompt Used (identical for both GenAIs)

The full prompt given to both agents specified:
- Implement a list-based (linked-list) MaxHeap from scratch — no array-based heap, no library priority-queue code.
- Priority order: Boss > Subordinate > Peer > ImportantPerson > OtherPerson; ties broken by newest date (MM-DD-YYYY) first.
- Support commands `EMAIL <category>,<subject>,<date>`, `NEXT`, `READ`, `COUNT` read from stdin, with the exact output formats specified in the assignment.
- Handle edge cases: `NEXT`/`READ` on an empty queue, repeated `NEXT` without `READ`, repeated `READ` without `NEXT`.
- Include the required file header block (Author/KUID/Lab/Date/Purpose) and adequate prologue + line/block comments per the rubric.
- Compile cleanly with `gcc <file>.c -o <exe>` using only `stdio.h`, `stdlib.h`, `string.h`.
- Verify against the assignment's sample test file and match its output exactly.

## 4. Generated Code From Both GenAIs

The complete source files from both subagents are included in this submission folder as:

- `GenAI1_Opus_Original.c` — full 512-line Claude Opus 5 output
- `GenAI2_Haiku_Original.c` — full 301-line Claude Haiku 4.5 output

Both files compiled cleanly (`gcc -Wall -Wextra`, zero warnings) and passed the assignment's sample test exactly, as generated. Abridged excerpts highlighting each program's core data-structure choice are reproduced below for readability; see the two files above for the full listings.

### 4a. Claude Opus 5 output

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_FIELD 512
#define NUM_CATEGORIES 5

static const char *CATEGORY_NAMES[NUM_CATEGORIES] = {
    "Boss", "Subordinate", "Peer", "ImportantPerson", "OtherPerson"
};

typedef struct Email {
    char sender[MAX_FIELD];
    char subject[MAX_FIELD];
    char date[MAX_FIELD];
    int rank;
    long dateKey;
} Email;

typedef struct HeapNode {
    Email data;
    struct HeapNode *next;
} HeapNode;

typedef struct MaxHeap {
    HeapNode *head;
    HeapNode *tail;
    int size;
} MaxHeap;
```

> See `GenAI1_Opus_Original.c` for the complete, fully-commented Opus-generated source.

### 4b. Claude Haiku 4.5 output

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    OtherPerson = 1, ImportantPerson = 2, Peer = 3, Subordinate = 4, Boss = 5
} SenderCategory;

typedef struct EmailNode {
    SenderCategory category;
    char *subject;
    char *date;
    struct EmailNode *next;
} EmailNode;

```

> See `GenAI2_Haiku_Original.c` for the complete 301-line source.

Both full listings were generated, compiled with `gcc -Wall -Wextra`, and verified against the assignment's sample test by each subagent independently.

## 5. Code Analysis

| Criterion | Claude Opus 5 | Claude Haiku 4.5 |
|---|---|---|
| **Correctness (spec conformance)** | Implements an actual binary **MaxHeap** on a linked list (level-order list, `(i-1)/2` parent, `2i+1`/`2i+2` children, `siftUp`/`siftDown`) — matches the assignment's explicit requirement to implement a MaxHeap. Sample test passed exactly on first verification except for one bug (see below). | Implements a **sorted linked list** (insertion sort on every `EMAIL`), not a heap — there is no parent/child tree structure or sift logic. It produces heap-like output (always-sorted, so the head is always max-priority) and passed the sample test exactly, but does not satisfy the literal requirement "implement a MaxHeap using a list-based implementation." |
| **Correctness (bug found)** | Printed a trailing blank line after the last `NEXT`/`COUNT` block of a run, producing one extra blank line the sample's expected output doesn't have when run to completion. | No bugs found in manual testing (sample test, empty-queue ops, repeated NEXT/READ, all 5 categories, date tie-break, malformed lines all handled correctly). |
| **Execution time** | Benchmarked with 3,000 `EMAIL` inserts followed by 3,000 `NEXT`/`READ` pairs (9,000 total commands): **0.054s** real time. Each insert does O(log n) sift steps, but each step calls `heapNodeAt()` which walks the list from `head` -> O(n) per step, so a single insert/removal is **O(n log n)** in the worst case, not the O(log n) of a true array-backed heap. | Same benchmark: **0.078s** real time. `insertEmail()` does a single O(n) list walk per insert (simple insertion sort), so insert/removal is **O(n)** — asymptotically *better* than Opus's list-based heap, because it avoids the repeated `heapNodeAt()` index-lookup overhead. `COUNT` is also O(n) (re-walks the list each call), vs. Opus's O(1) (cached `size` field). |
| **Space complexity** | Both are O(n) in the number of emails. Opus uses **fixed-size** `char[512]` arrays for sender/subject/date embedded directly in each node (one `malloc` per node, ~1.5KB+ per email, no fragmentation, but wastes space for short fields). | Haiku uses **exact-size** `malloc`'d buffers for subject/date (three `malloc` calls per node: node + subject + date), which is more space-efficient per email but does more allocator bookkeeping and is more error-prone to free correctly (though it does so correctly here). |
| **Maintainability** | ~512 lines. Very thorough section banners, a full comment on every function, and prototypes centralized at the top. The `heapNodeAt()` linear-index abstraction is textbook-heap in shape but is a known anti-pattern for linked-list heaps (it re-derives what array indexing gets for free). A maintainer extending this to a larger heap should recognize the O(n) index lookup as the thing to optimize first. | ~301 lines, more concise, uses a `SenderCategory` enum which is arguably more self-documenting/type-safe than Opus's string-table + integer-rank approach. However, because it's structurally an insertion-sorted list masquerading as a "heap", a future maintainer reading only the type names (`MaxHeap`... it doesn't even have one — it's just `EmailNode *heap`) could be misled about what data structure is actually in play, which is a maintainability/documentation  concern. |

## 6. Selection and Justification

**Selected: Claude Opus 5's implementation**, as the basis for the final submission.

**Justification:** The assignment states explicitly: *"You must implement a MaxHeap using a list-based implementation. Then use that MaxHeap to handle all your email prioritizing."* Opus's program is a real binary max-heap stored in a linked list. Haiku's program, while it produces correct output on every test performed (including the graded sample and multiple hand-constructed edge cases), is structurally an insertion-sorted linked list. Since the assignment is graded in part on the data structure itself (not just black-box output), correctness outweighs Haiku's better insert complexity and slightly faster real-world benchmark margin was negligible (0.054s vs 0.078s on 3,000 emails) is not a practically meaningful difference.

## 7. Improvements Made (Correctness, Execution Time, Space Complexity, Maintainability)

Starting from the selected Opus draft, the following changes were made:

- **Correctness fix:** `printNext()` and `printCount()` originally printed a trailing `"\n"` after every output block. This is functionally equivalent to a leading separator for every case *except* the very last command in a run, where it leaves one extra blank line at the end of the program's output that the assignment's expected output does not have. The author replaced this with a `firstOutputPrinted` static flag: a blank line is now printed **before** a block only if a previous block has already been printed, which reproduces the sample's exact output (verified with `diff` — byte-for-byte match) regardless of which command ends the run.
- **Execution time / space complexity:** No structural changes were made here — the O(n log n) insert/remove cost from `heapNodeAt()`'s linear index lookup was identified (Section 5) as a known limitation of a "textbook" linked-list heap, but changing it (e.g., to a doubly-linked list with cached parent pointers, or switching to an array-backed heap) was judged out of scope: the assignment explicitly calls for a *list-based* implementation, and at the scale of one CEO's email inbox (dozens to low hundreds of emails, not millions) the difference between O(n) and O(n log n) per operation is not observable in practice, as the benchmark in Section 5 shows.
- **Maintainability:** Updated the file's prologue comment to correctly disclose GenAI as the origin of the initial draft, list the author's specific revision (with a `Revisions:` line per the rubric's required prologue fields), and correct a stale `Compile:` comment that still referenced the throwaway `genai_opus.c`/`genai_opus` filenames instead of the submitted `email_prioritizer.c`/`email_prioritizer`.
