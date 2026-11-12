/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */

/**
 * @brief Finds the buffer entry containing a specified character offset.
 *
 * The circular buffer entries are treated as one continuous sequence
 * of bytes, starting from the oldest entry.
 *
 * @param buffer Pointer to the circular buffer.
 * @param char_offset Zero-based byte position in the combined data.
 * @param entry_offset_byte_rtn Receives the byte offset within
 *                              the matching entry.
 *
 * @return Pointer to the matching entry, or NULL if not found.
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer,
    size_t char_offset,
    size_t *entry_offset_byte_rtn)
{
    // Start searching from the oldest entry.
    uint8_t index = buffer->out_offs;

    // Determine how many valid entries are currently stored.
    uint8_t count;

    if (buffer->full) {
        // A full circular buffer contains all 10 entries.
        count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } else {
        // Calculate the number of entries, accounting for wraparound.
        count = (buffer->in_offs +
                 AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED -
                 buffer->out_offs) %
                 AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Examine each valid entry, starting with the oldest.
    for (uint8_t i = 0; i < count; i++) {

        struct aesd_buffer_entry *entry = &buffer->entry[index];

        // Check whether the requested offset belongs to this entry.
        if (char_offset < entry->size) {

            // Store the byte position within the matching entry.
            *entry_offset_byte_rtn = char_offset;

            // Return the entry containing the requested character.
            return entry;
        }

        // Skip this entry's bytes to find the relative offset
        // within the next entry.
        char_offset -= entry->size;

        // Move to the next entry, wrapping around after index 9.
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // The requested offset exceeds the total stored data size.
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)

{
    // Copy the new entry into the current write position.
    buffer->entry[buffer->in_offs] = *add_entry;

    // If the buffer is full, the oldest entry is being overwritten.
    // Advance the read position to the next oldest entry.
    if (buffer->full) {
        buffer->out_offs =
            (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Advance the write position to the next slot.
    // Modulo ensures the index wraps from 9 back to 0.
    buffer->in_offs =
        (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // If the write position catches up with the read position,
    // all available slots are occupied.
    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
