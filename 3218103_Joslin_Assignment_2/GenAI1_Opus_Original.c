#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Fixed sizes and category ranking constants
 * -------------------------------------------------------------------------- */
#define MAX_LINE 1024   /* maximum length of one input line                  */
#define MAX_FIELD 512   /* maximum length of one comma-delimited field       */
#define NUM_CATEGORIES 5

/* Sender categories listed from highest priority to lowest priority.  The
 * rank of a category is derived from its position in this table, so the table
 * order is the single place that defines the category ordering. */
static const char *CATEGORY_NAMES[NUM_CATEGORIES] = {
    "Boss",            /* rank 5 - highest */
    "Subordinate",     /* rank 4 */
    "Peer",            /* rank 3 */
    "ImportantPerson", /* rank 2 */
    "OtherPerson"      /* rank 1 - lowest  */
};

/* --------------------------------------------------------------------------
 * Email: the payload stored in every heap node.
 * -------------------------------------------------------------------------- */
typedef struct Email {
    char sender[MAX_FIELD];  /* sender category string, exactly as supplied  */
    char subject[MAX_FIELD]; /* subject line (may contain spaces)            */
    char date[MAX_FIELD];    /* date string in MM-DD-YYYY form               */
    int rank;                /* numeric category priority: 5 = Boss .. 1     */
    long dateKey;            /* YYYYMMDD as an integer for easy comparison   */
} Email;

/* --------------------------------------------------------------------------
 * HeapNode / MaxHeap: the list-based heap itself.
 *
 * The heap is a complete binary tree stored as a singly linked list in
 * level order.  Node 0 is the root; for the node at position i the parent
 * lives at (i - 1) / 2 and the children live at 2i + 1 and 2i + 2.  Those
 * positions are reached by walking the list, which is what makes this a
 * list-based rather than an array-based implementation.
 * -------------------------------------------------------------------------- */
typedef struct HeapNode {
    Email data;             /* the email held by this node                   */
    struct HeapNode *next;  /* next node in level order                      */
} HeapNode;

typedef struct MaxHeap {
    HeapNode *head;         /* first node (the root of the heap)             */
    HeapNode *tail;         /* last node in level order, for O(1) appends    */
    int size;               /* number of emails currently in the heap        */
} MaxHeap;

/* --------------------------------------------------------------------------
 * Function prototypes
 * -------------------------------------------------------------------------- */
static void heapInit(MaxHeap *heap);
static void heapDestroy(MaxHeap *heap);
static HeapNode *heapNodeAt(const MaxHeap *heap, int index);
static void heapInsert(MaxHeap *heap, Email email);
static int heapPeek(const MaxHeap *heap, Email *out);
static void heapRemoveTop(MaxHeap *heap);
static void siftUp(MaxHeap *heap, int index);
static void siftDown(MaxHeap *heap, int index);
static int higherPriority(const Email *a, const Email *b);
static void swapEmails(Email *a, Email *b);
static int categoryRank(const char *category);
static long dateToKey(const char *date);
static void trim(char *text);
static int splitEmailFields(const char *body, char *sender, char *subject, char *date);
static void handleEmailCommand(MaxHeap *heap, const char *body);
static void printNext(const MaxHeap *heap);
static void printCount(const MaxHeap *heap);

/* ==========================================================================
 * Heap construction and teardown
 * ========================================================================== */

/* Put a heap into the well-defined empty state. */
static void heapInit(MaxHeap *heap)
{
    heap->head = NULL;
    heap->tail = NULL;
    heap->size = 0;
}

/* Free every node in the heap so the program leaks no memory. */
static void heapDestroy(MaxHeap *heap)
{
    HeapNode *current = heap->head;

    while (current != NULL) {
        HeapNode *doomed = current; /* remember the node being released      */
        current = current->next;    /* advance before freeing                */
        free(doomed);
    }

    heapInit(heap);
}

/* ==========================================================================
 * Positional access: the bridge between heap index arithmetic and the list
 * ========================================================================== */

/* Walk the list and return the node sitting at the given level-order index,
 * or NULL when the index is outside the heap. */
static HeapNode *heapNodeAt(const MaxHeap *heap, int index)
{
    HeapNode *current = heap->head;
    int position = 0;

    if (index < 0 || index >= heap->size) {
        return NULL; /* index does not exist in this heap                    */
    }

    while (current != NULL && position < index) {
        current = current->next;
        position++;
    }

    return current;
}

/* ==========================================================================
 * Priority comparison helpers
 * ========================================================================== */

/* Translate a sender category string into its numeric rank.  Unknown
 * categories rank below every known category so that bad input can never
 * outrank a legitimate email. */
static int categoryRank(const char *category)
{
    int i;

    for (i = 0; i < NUM_CATEGORIES; i++) {
        if (strcmp(category, CATEGORY_NAMES[i]) == 0) {
            /* First entry is the highest rank, hence the subtraction. */
            return NUM_CATEGORIES - i;
        }
    }

    return 0; /* unrecognized category                                       */
}

/* Convert an MM-DD-YYYY date string into a single comparable integer of the
 * form YYYYMMDD.  Digits are gathered in order and the separators are simply
 * ignored, so the routine is tolerant of stray spaces around the date. */
static long dateToKey(const char *date)
{
    int digits[8]; /* MM DD YYYY = 8 digits                                  */
    int found = 0;
    int i;
    long month = 0;
    long day = 0;
    long year = 0;

    for (i = 0; date[i] != '\0' && found < 8; i++) {
        if (date[i] >= '0' && date[i] <= '9') {
            digits[found++] = date[i] - '0';
        }
    }

    if (found < 8) {
        return 0; /* malformed date: treat as the oldest possible date       */
    }

    month = digits[0] * 10L + digits[1];
    day = digits[2] * 10L + digits[3];
    year = ((digits[4] * 10L + digits[5]) * 10L + digits[6]) * 10L + digits[7];

    return year * 10000L + month * 100L + day; /* YYYYMMDD ordering          */
}

/* Return 1 when email a must sit above email b in the heap.  Category rank
 * dominates; a tie is broken in favour of the newer date. */
static int higherPriority(const Email *a, const Email *b)
{
    if (a->rank != b->rank) {
        return a->rank > b->rank;
    }

    return a->dateKey > b->dateKey; /* newest email wins the tie             */
}

/* Exchange the payloads of two nodes.  Only the data moves, never the links,
 * which keeps the level-order structure of the list intact. */
static void swapEmails(Email *a, Email *b)
{
    Email temp = *a;
    *a = *b;
    *b = temp;
}

/* ==========================================================================
 * Core heap operations
 * ========================================================================== */

/* Restore the heap property by moving a node upward toward the root. */
static void siftUp(MaxHeap *heap, int index)
{
    while (index > 0) {
        int parentIndex = (index - 1) / 2;
        HeapNode *child = heapNodeAt(heap, index);
        HeapNode *parent = heapNodeAt(heap, parentIndex);

        if (child == NULL || parent == NULL) {
            return; /* defensive: should not happen for valid indices        */
        }

        /* Stop as soon as the child no longer outranks its parent. */
        if (!higherPriority(&child->data, &parent->data)) {
            return;
        }

        swapEmails(&child->data, &parent->data);
        index = parentIndex; /* continue from the parent's position          */
    }
}

/* Restore the heap property by moving a node downward toward the leaves. */
static void siftDown(MaxHeap *heap, int index)
{
    while (1) {
        int leftIndex = 2 * index + 1;
        int rightIndex = 2 * index + 2;
        int bestIndex = index;
        HeapNode *bestNode = heapNodeAt(heap, index);
        HeapNode *leftNode = heapNodeAt(heap, leftIndex);
        HeapNode *rightNode = heapNodeAt(heap, rightIndex);
        HeapNode *chosenNode = NULL;

        if (bestNode == NULL) {
            return; /* nothing to sift                                       */
        }

        /* Pick whichever of the node and its two children ranks highest. */
        if (leftNode != NULL && higherPriority(&leftNode->data, &bestNode->data)) {
            bestIndex = leftIndex;
            bestNode = leftNode;
        }
        if (rightNode != NULL && higherPriority(&rightNode->data, &bestNode->data)) {
            bestIndex = rightIndex;
            bestNode = rightNode;
        }

        if (bestIndex == index) {
            return; /* the heap property already holds here                  */
        }

        chosenNode = heapNodeAt(heap, index);
        swapEmails(&chosenNode->data, &bestNode->data);
        index = bestIndex; /* follow the email that was pushed down          */
    }
}

/* Add one email to the heap: append it as the new last node in level order
 * and then sift it up into its correct place. */
static void heapInsert(MaxHeap *heap, Email email)
{
    HeapNode *node = (HeapNode *)malloc(sizeof(HeapNode));

    if (node == NULL) {
        return; /* out of memory: drop the email rather than crash           */
    }

    node->data = email;
    node->next = NULL;

    if (heap->head == NULL) {
        heap->head = node; /* first node becomes the root                    */
    } else {
        heap->tail->next = node;
    }
    heap->tail = node;
    heap->size++;

    siftUp(heap, heap->size - 1); /* bubble the newcomer up                  */
}

/* Copy the highest-priority email into *out.  Returns 0 for an empty heap. */
static int heapPeek(const MaxHeap *heap, Email *out)
{
    if (heap->size == 0 || heap->head == NULL) {
        return 0;
    }

    *out = heap->head->data;
    return 1;
}

/* Remove the highest-priority email.  The last node's payload is moved into
 * the root, the last node is unlinked and freed, and the new root is sifted
 * down.  Removing from an empty heap does nothing. */
static void heapRemoveTop(MaxHeap *heap)
{
    HeapNode *last = NULL;

    if (heap->size == 0 || heap->head == NULL) {
        return; /* empty queue: silently ignore                              */
    }

    if (heap->size == 1) {
        free(heap->head); /* the root is also the last node                  */
        heapInit(heap);
        return;
    }

    last = heap->tail;
    heap->head->data = last->data;        /* promote the last email          */
    heap->tail = heapNodeAt(heap, heap->size - 2); /* new last node          */
    heap->tail->next = NULL;              /* detach the node being removed   */
    free(last);
    heap->size--;

    siftDown(heap, 0); /* push the promoted email back down                  */
}

/* ==========================================================================
 * Input parsing helpers
 * ========================================================================== */

/* Remove leading and trailing whitespace from a string in place. */
static void trim(char *text)
{
    size_t start = 0;
    size_t end;
    size_t length = strlen(text);

    while (start < length &&
           (text[start] == ' ' || text[start] == '\t' ||
            text[start] == '\r' || text[start] == '\n')) {
        start++;
    }

    end = length;
    while (end > start &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' ||
            text[end - 1] == '\r' || text[end - 1] == '\n')) {
        end--;
    }

    /* Shift the surviving characters to the front and terminate. */
    memmove(text, text + start, end - start);
    text[end - start] = '\0';
}

/* Split the body of an EMAIL command ("category,subject,date") into its three
 * fields.  The subject may contain spaces but no commas, so splitting on the
 * first and last comma is unambiguous.  Any spaces that follow a comma are
 * trimmed away, so both "A,B,C" and "A, B, C" parse identically.
 * Returns 1 on success and 0 when the body is malformed. */
static int splitEmailFields(const char *body, char *sender, char *subject, char *date)
{
    const char *firstComma = strchr(body, ',');
    const char *secondComma = NULL;
    size_t senderLength;
    size_t subjectLength;

    if (firstComma == NULL) {
        return 0; /* no field separators at all                              */
    }

    secondComma = strchr(firstComma + 1, ',');
    if (secondComma == NULL) {
        return 0; /* only two fields supplied                                */
    }

    senderLength = (size_t)(firstComma - body);
    subjectLength = (size_t)(secondComma - (firstComma + 1));

    /* Guard against overrunning the destination buffers. */
    if (senderLength >= MAX_FIELD || subjectLength >= MAX_FIELD ||
        strlen(secondComma + 1) >= MAX_FIELD) {
        return 0;
    }

    memcpy(sender, body, senderLength);
    sender[senderLength] = '\0';
    memcpy(subject, firstComma + 1, subjectLength);
    subject[subjectLength] = '\0';
    strcpy(date, secondComma + 1);

    trim(sender);
    trim(subject);
    trim(date);

    return 1;
}

/* Build an Email from the text of an EMAIL command and insert it. */
static void handleEmailCommand(MaxHeap *heap, const char *body)
{
    Email email;
    char sender[MAX_FIELD];
    char subject[MAX_FIELD];
    char date[MAX_FIELD];

    if (!splitEmailFields(body, sender, subject, date)) {
        return; /* ignore malformed EMAIL lines                              */
    }

    strcpy(email.sender, sender);
    strcpy(email.subject, subject);
    strcpy(email.date, date);
    email.rank = categoryRank(sender);   /* precompute the category priority */
    email.dateKey = dateToKey(date);     /* precompute the sortable date     */

    heapInsert(heap, email);
}

/* ==========================================================================
 * Output helpers
 * ========================================================================== */

/* Print the highest-priority email, or nothing at all when none exists.
 * Each report is followed by a blank line so consecutive reports stay
 * visually separated. */
static void printNext(const MaxHeap *heap)
{
    Email top;

    if (!heapPeek(heap, &top)) {
        return; /* empty queue produces no output                            */
    }

    printf("Next email:\n");
    printf("\tSender: %s\n", top.sender);
    printf("\tSubject: %s\n", top.subject);
    printf("\tDate: %s\n", top.date);
    printf("\n");
}

/* Print how many emails remain unread, followed by a blank line. */
static void printCount(const MaxHeap *heap)
{
    printf("There are %d emails to read.\n", heap->size);
    printf("\n");
}

/* ==========================================================================
 * main: read commands until end of input and dispatch each one
 * ========================================================================== */
int main(void)
{
    MaxHeap heap;
    char line[MAX_LINE];

    heapInit(&heap);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *command = line;

        trim(command); /* drop the newline and any stray whitespace          */

        if (command[0] == '\0') {
            continue; /* skip blank lines                                    */
        }

        /* Dispatch on the command keyword.  EMAIL carries a payload, the
         * other three commands stand alone. */
        if (strncmp(command, "EMAIL", 5) == 0) {
            handleEmailCommand(&heap, command + 5); /* skip past "EMAIL"     */
        } else if (strcmp(command, "NEXT") == 0) {
            printNext(&heap);
        } else if (strcmp(command, "READ") == 0) {
            heapRemoveTop(&heap); /* silent removal                          */
        } else if (strcmp(command, "COUNT") == 0) {
            printCount(&heap);
        }
        /* Unrecognized commands are ignored. */
    }

    heapDestroy(&heap); /* release every remaining node                      */
    return 0;
}
