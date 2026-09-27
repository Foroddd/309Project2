# Design Log — Project 2

(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost

Conversation uses a growth factor of 2. An empty conversation does not have a buffer. When the buffer is full, append creates a new buffer with twice the capacity, moves the messages over, deletes the old buffer, and adds the new message. The capacities are 1, 2, 4, 8, etc.

Claim: n appends take O(n) total time, so each append is O(1) amortized.

Proof: A reallocation happens when the size is 0 or a power of two. The number of elements moved is:
1+ 2 + 4 + ... + 2^k = 2^(k+1) - 1 < 2n

There are also n writes for the new messages. This means n appends take less than 3n element operations, which is O(n) total, so each append is O(1) amortized.

Another way of thinking about it is to charge each append 3 credits, one credit pays for adding the new element, and the other two are saved for the next reallocation. This gives enough credits to move all the elements when the buffer grows

Why 2: A growth factor of 2 keeps the number of reallocations small. For example, 1,025 appends only require 12 reallocations. A smaller factor uses less extra memory but causes more reallocations. Growing by a fixed amount would require O(n^2) work. The GrowthDoublesCapacity test checks that the buffer grows at sizes 0, 1, 2, 4, etc.

## Rule of Five evidence

Destructor: Uses delete[] data_. Deleting nullptr is safe, so empty and moved-from conversations are safe.

Copy constructor: Creates a new buffer with exactly enough space for the messages and copies each message. If copying fails, the new buffer is deleted before the error is thrown.

Copy assignment: Uses copy-and-swap. It first creates a copy and then swaps it with the current object. If copying fails, the original object is unchanged, it also handles self-assignment.

Move constructor and move assignment: Take the existing buffer from the other conversation instead of copying the messages, the old conversation is then set to nullptr, 0, and 0. Move assignment also deletes the target's old buffer before taking the new one

append takes its argument by value. This makes something like c.append(c.at(0)) safe even if the append causes the buffer to be reallocated

The tests were run with AddressSanitizer and UBSan with no memory leaks. I also tested some broken versions of the code. A shallow copy failed the pointer test, removing delete[] caused a memory leak, and not clearing the source after a move caused the move tests to fail.

## Sentinel scanner: bounded pending_ proof
Say S be the length of the sentinel, which is 20.

The main rule is that pending_ is always a proper prefix of the sentinel, this means pending_ can never be longer than S - 1.

At the start, pending_ is empty. When feed is called, the scanner combines pending_ with the new chunk

If the sentinel is found, pending_ is cleared

If the sentinel is not found, pending_ becomes the longest ending part of the text that could still be the beginning of the sentinel. Its length can never be more than S - 1. If it were S characters long, the complete sentinel would have been found

flush releases anything still stored in pending_ because there is no more input for it to become a complete sentinel

This makes sure the scanner does not print characters that could still be part of the sentinel.

Each feed only needs to check the new chunk plus at most S - 1 characters from the previous chunk. This means the scanner uses O(S) memory instead of storing the entire input.

The ScannerBoundedMemoryOnAdversarialStream test feeds 4 MB of data one byte at a time and checks that pending_ never becomes larger than 19 characters. It also makes sure every byte is eventually output.


## What I would change differently

The public interface does not provide direct access to capacity_ or pending_, so the tests have to estimate them.

For capacity_, the tests check if begin() changes, if the buffer is reallocated, the new buffer has a different address

For pending_, the tests calculate the number of bytes fed minus the number of bytes already output

I would add read only capacity() and pending_size() functions so the tests could check these values directly