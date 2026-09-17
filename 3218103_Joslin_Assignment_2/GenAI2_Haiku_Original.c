#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Email Prioritization System using List-Based MaxHeap
 *
 * Description: Implements a priority queue for managing CEO emails using a linked list
 * that maintains max-heap property. Emails are prioritized by sender category, with
 * date serving as a tie-breaker.
 *
 * Inputs: Commands via stdin (EMAIL, NEXT, READ, COUNT)
 * Outputs: Email data and status messages to stdout
 *
 * Author: David Joslin
 * Creation Date: 09/17/2026
 * Revision Date: 09/17/2026
 * Sources: Assignment specification
 * Collaborators: None
 */

/* Define sender categories with priority values (higher = higher priority) */
typedef enum {
    OtherPerson = 1,
    ImportantPerson = 2,
    Peer = 3,
    Subordinate = 4,
    Boss = 5
} SenderCategory;

/* Email node structure for linked list */
typedef struct EmailNode {
    SenderCategory category;
    char *subject;
    char *date;  /* MM-DD-YYYY format */
    struct EmailNode *next;
} EmailNode;

/*
 * getCategory - Convert category string to enum value
 * Input: categoryStr - string representation of sender category
 * Output: SenderCategory enum value
 */
SenderCategory getCategory(const char *categoryStr) {
    if (strcmp(categoryStr, "Boss") == 0) return Boss;
    if (strcmp(categoryStr, "Subordinate") == 0) return Subordinate;
    if (strcmp(categoryStr, "Peer") == 0) return Peer;
    if (strcmp(categoryStr, "ImportantPerson") == 0) return ImportantPerson;
    if (strcmp(categoryStr, "OtherPerson") == 0) return OtherPerson;
    return OtherPerson;  /* default */
}

/*
 * categoryToString - Convert category enum to string representation
 * Input: cat - SenderCategory enum value
 * Output: String representation of category
 */
const char *categoryToString(SenderCategory cat) {
    switch (cat) {
        case Boss: return "Boss";
        case Subordinate: return "Subordinate";
        case Peer: return "Peer";
        case ImportantPerson: return "ImportantPerson";
        case OtherPerson: return "OtherPerson";
        default: return "Unknown";
    }
}

/*
 * compareDates - Compare two dates in MM-DD-YYYY format
 * Input: date1, date2 - dates in MM-DD-YYYY format
 * Output: 1 if date1 > date2, -1 if date1 < date2, 0 if equal
 */
int compareDates(const char *date1, const char *date2) {
    int m1, d1, y1, m2, d2, y2;
    /* Parse dates in MM-DD-YYYY format */
    sscanf(date1, "%d-%d-%d", &m1, &d1, &y1);
    sscanf(date2, "%d-%d-%d", &m2, &d2, &y2);

    /* Compare year first, then month, then day */
    if (y1 != y2) return (y1 > y2) ? 1 : -1;
    if (m1 != m2) return (m1 > m2) ? 1 : -1;
    if (d1 != d2) return (d1 > d2) ? 1 : -1;
    return 0;
}

/*
 * compareEmails - Compare priority of two emails for max-heap ordering
 * Input: email1, email2 - EmailNode pointers to compare
 * Output: 1 if email1 > email2, -1 if email1 < email2, 0 if equal
 * Logic: Primary sort by category (higher priority first),
 *        secondary sort by date (newer first for same category)
 */
int compareEmails(EmailNode *email1, EmailNode *email2) {
    /* First compare by category (higher priority value wins) */
    if (email1->category != email2->category) {
        return (email1->category > email2->category) ? 1 : -1;
    }
    /* Same category: newer date has higher priority */
    return compareDates(email1->date, email2->date);
}

/*
 * createEmailNode - Allocate and initialize new email node
 * Input: category - sender category, subject - email subject, date - email date
 * Output: Pointer to newly created EmailNode or NULL on allocation failure
 */
EmailNode *createEmailNode(SenderCategory category, const char *subject, const char *date) {
    EmailNode *node = (EmailNode *)malloc(sizeof(EmailNode));
    if (node == NULL) return NULL;

    node->category = category;
    node->subject = (char *)malloc(strlen(subject) + 1);
    node->date = (char *)malloc(strlen(date) + 1);

    if (node->subject == NULL || node->date == NULL) {
        free(node->subject);
        free(node->date);
        free(node);
        return NULL;
    }

    strcpy(node->subject, subject);
    strcpy(node->date, date);
    node->next = NULL;

    return node;
}

/*
 * freeEmailNode - Deallocate email node and its fields
 * Input: node - pointer to EmailNode to free
 * Output: None
 */
void freeEmailNode(EmailNode *node) {
    if (node != NULL) {
        free(node->subject);
        free(node->date);
        free(node);
    }
}

/*
 * insertEmail - Insert email into heap while maintaining max-heap property
 * Input: head - pointer to heap head pointer, newNode - new email node to insert
 * Output: None (modifies heap in-place)
 * Logic: Maintains sorted order by email priority. List stays sorted so
 *        the head always points to the maximum priority email.
 */
void insertEmail(EmailNode **head, EmailNode *newNode) {
    if (*head == NULL) {
        *head = newNode;
        return;
    }

    /* If new node is better than head, insert at head */
    if (compareEmails(newNode, *head) > 0) {
        newNode->next = *head;
        *head = newNode;
        return;
    }

    /* Find correct insertion position maintaining sorted order */
    EmailNode *current = *head;
    while (current->next != NULL && compareEmails(newNode, current->next) <= 0) {
        current = current->next;
    }

    newNode->next = current->next;
    current->next = newNode;
}

/*
 * removeTop - Remove and return the highest priority email
 * Input: head - pointer to heap head pointer
 * Output: Pointer to removed top email, or NULL if heap is empty
 */
EmailNode *removeTop(EmailNode **head) {
    if (*head == NULL) return NULL;

    EmailNode *top = *head;
    *head = (*head)->next;
    top->next = NULL;
    return top;
}

/*
 * getEmailCount - Count unread emails in heap
 * Input: head - pointer to heap head
 * Output: Integer count of emails
 */
int getEmailCount(EmailNode *head) {
    int count = 0;
    while (head != NULL) {
        count++;
        head = head->next;
    }
    return count;
}

/*
 * freeAllEmails - Deallocate all emails in heap
 * Input: head - pointer to heap head pointer
 * Output: None (heap becomes empty)
 */
void freeAllEmails(EmailNode **head) {
    while (*head != NULL) {
        EmailNode *temp = removeTop(head);
        freeEmailNode(temp);
    }
}

/*
 * main - Main program loop
 * Reads commands from stdin and performs email prioritization operations
 * Supported commands: EMAIL, NEXT, READ, COUNT
 */
int main() {
    EmailNode *heap = NULL;
    char line[1024];
    int firstOutput = 1;  /* Track if this is the first output */

    /* Main command processing loop */
    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Remove trailing newline */
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "EMAIL ", 6) == 0) {
            /* ===== Parse EMAIL command ===== */
            char *data = line + 6;
            char *commaPos1 = strchr(data, ',');
            if (commaPos1 == NULL) continue;

            /* Extract sender category (first field) */
            int categoryLen = commaPos1 - data;
            char categoryStr[256];
            strncpy(categoryStr, data, categoryLen);
            categoryStr[categoryLen] = '\0';

            /* Find second comma separator */
            char *commaPos2 = strchr(commaPos1 + 1, ',');
            if (commaPos2 == NULL) continue;

            /* Extract subject line (second field) */
            int subjectLen = commaPos2 - (commaPos1 + 1);
            char subject[1024];
            strncpy(subject, commaPos1 + 1, subjectLen);
            subject[subjectLen] = '\0';

            /* Extract date (third field) */
            char date[256];
            strcpy(date, commaPos2 + 1);

            /* Create and insert new email */
            SenderCategory category = getCategory(categoryStr);
            EmailNode *newNode = createEmailNode(category, subject, date);
            if (newNode != NULL) {
                insertEmail(&heap, newNode);
            }
        }
        else if (strcmp(line, "NEXT") == 0) {
            /* ===== Handle NEXT command: display top email ===== */
            if (heap != NULL) {
                printf("\nNext email:\n");
                printf("\tSender: %s\n", categoryToString(heap->category));
                printf("\tSubject: %s\n", heap->subject);
                printf("\tDate: %s\n", heap->date);
                firstOutput = 0;
            }
        }
        else if (strcmp(line, "READ") == 0) {
            /* ===== Handle READ command: remove top email ===== */
            EmailNode *removed = removeTop(&heap);
            if (removed != NULL) {
                freeEmailNode(removed);
            }
        }
        else if (strcmp(line, "COUNT") == 0) {
            /* ===== Handle COUNT command: display email count ===== */
            int count = getEmailCount(heap);
            if (!firstOutput) {
                printf("\n");
            }
            printf("There are %d emails to read.\n", count);
            firstOutput = 0;
        }
    }

    /* Clean up: deallocate remaining emails */
    freeAllEmails(&heap);

    return 0;
}
