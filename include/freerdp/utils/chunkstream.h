/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2020 Hardening <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_UTILS_CHUNKSTREAM_H
#define FREERDP_UTILS_CHUNKSTREAM_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <freerdp/api.h>

typedef struct _ChunkStream ChunkStream;
typedef struct _ChunkStreamSlot ChunkStreamSlot;

#define CHUNKSTREAM_MAX_SLOTS 50

#ifdef __cplusplus
extern "C"
{
#endif
	/**
	 * A ChunkStream is a component to prevent multiple memory copies when forging a packet from
	 * headers to the payload. The idea is to create a list of chunks (slots) and to do the
	 * assembly of the full content only when needed (usually when we want to send the content over
	 * the network).
	 *
	 * The chunks can come from static blobs (chunkstream_getStaticStringSlot or
	 * chunkstream_getStaticMemSlot), from memory allocated by the user (chunkstream_getMallocSlot)
	 * or from a limited pool allocated by the chunkstream (chunkstream_getPoolSlot).
	 *
	 * chunkstream_sizeAfterSlot is useful to compute the size of chunks after a certain slot,
	 * typically when you want to write some headers that inform of the size of the following
	 * payload.
	 *
	 * A full example usage with a protocol with at least 2 layers:
	 * 		void layer2(ChunkStream *cs)
	 * 		{
	 * 			... allocate and write some more chunks ...
	 * 		}
	 *
	 * 		void layer1(ChunkStream *cs)
	 * 		{
	 * 			wStream layer1Headers;
	 * 			// headers of layers 1 are at max 20 bytes
	 * 			ChunkStreamSlot *headersSlot = chunkstream_getPoolStream(cs, 20, &layer1Headers);
	 *
	 * 			layers2(cs); // create payload, will adds chunks in the chunkstream
	 *
	 * 			int payloadSz = chunkstream_sizeAfterSlot(cs, headersSlot);
	 *
	 * 			writeLayer1Headers(layer1Headers, payloadSz);
	 *
	 * 			chunkstreamslot_update_fromStream(headersSlot, &layer1Headers);
	 * 		}
	 *
	 * 		void writePacket()
	 * 		{
	 * 			ChunkStream *cs = chunkstream_new(1024);
	 * 			layer1(cs);
	 *
	 * 			wStream *s = chunkstream_linearizeToStream(cs);
	 *
	 * 			... serialize the content of s ...
	 * 		}
	 */

	/** Initialises a chunkstream
	 * @param initialSize the initial capacity of the chunkstream's pool
	 * @return if the created chunkstream
	 */
	FREERDP_API ChunkStream* chunkstream_new(size_t initialSize);

	/** Destroys internal data used by this chunkstream
	 * @param pchunkstream a pointer to a chunkstream
	 */
	FREERDP_API void chunkstream_destroy(ChunkStream** pchunkstream);

	/**
	 * Retrieves a slot containing a static string
	 * @param chunkstream the chunkstream
	 * @param str the string to add
	 * @param include0 tells if we should include the leading null byte
	 * @return the corresponding ChunkStreamSlot, NULL if it failed (no available slots)
	 */
	FREERDP_API ChunkStreamSlot* chunkstream_getStaticStringSlot(ChunkStream* chunkstream,
	                                                             const char* str, BOOL include0);

	/**
	 * Retrieves a slot containing a static memory blob
	 * @param chunkstream the chunkstream
	 * @param data the memory blob to add
	 * @param sz size of the memory blob
	 * @return the corresponding ChunkStreamSlot, NULL if it failed (no available slots)
	 */
	FREERDP_API ChunkStreamSlot* chunkstream_getStaticMemSlot(ChunkStream* chunkstream,
	                                                          const BYTE* data, size_t sz);

	/**
	 * Retrieves a slot containing a memory blob alloced by the user, the chunkstream takes
	 * ownership of the provided memory, so it will be freed when the chunkstream is destroyed.
	 * @param chunkstream the chunkstream
	 * @param data the memory blob to add
	 * @param sz size of the memory blob
	 * @return the corresponding ChunkStreamSlot, NULL if it failed (no available slots)
	 */
	FREERDP_API ChunkStreamSlot* chunkstream_getMallocSlot(ChunkStream* chunkstream, BYTE* data,
	                                                       size_t sz);

	/**
	 * Retrieves a slot from the chunkstream's pool. This slot is taken from the pool with an empty
	 * used size, so it's usually a good idea to use chunkstreamslot_update_used() to set the space
	 * used in the chunk.
	 * @param chunkstream the chunkstream
	 * @param sz size to retrieve from the pool
	 * @return the corresponding ChunkStreamSlot, NULL if fails (no available slots, or memory in
	 * 		 the pool)
	 */
	FREERDP_API ChunkStreamSlot* chunkstream_getPoolSlot(ChunkStream* chunkstream, size_t sz);

	/**
	 * Retrieves a slot from the chunkstream pool and initialize a static stream with it. The
	 * stream will point on the memory associated with the pool item. It's usually a good idea
	 * to call chunkstreamslot_update_fromStream() when stream processing is finished, to update
	 * the used counter of the chunk.
	 * @param chunkstream the chunkstream
	 * @param sz size to retrieve from the pool
	 * @param s the stream to initialize
	 * @return the corresponding ChunkStreamSlot, NULL if fails (no available slots, or memory in
	 * 			the pool)
	 */
	FREERDP_API ChunkStreamSlot* chunkstream_getPoolStream(ChunkStream* chunkstream, size_t sz,
	                                                       wStream* s);

	/**
	 * Assembles all the chunks and put them in the returned wStream
	 * @param chunkstream the chunkstream
	 * @return a new wStream containing the assembled chunks
	 */
	FREERDP_API wStream* chunkstream_linearizeToStream(ChunkStream* chunkstream);

	/**
	 * Assemble all the chunks in a provided wStream. The function ensures that there's enough
	 * remaining space in the target stream.
	 * @param chunkstream the chunkstream
	 * @param s the target stream
	 * @return if the operation was successful
	 */
	FREERDP_API BOOL chunkstream_linearizeInStream(ChunkStream* chunkstream, wStream* s);

	/**
	 * Retrieves the size stored in the chunkstream after a given slot
	 * @param chunkstream the chunkstream
	 * @param slot the slot to start to compute
	 * @return the stored size, -1 if the slot was not found
	 */
	FREERDP_API int chunkstream_sizeAfterSlot(const ChunkStream* chunkstream,
	                                          const ChunkStreamSlot* slot);

	/**
	 * returns the size of the slot
	 * @param slot the slot
	 * @return the size of the slot
	 */
	FREERDP_API size_t chunkstreamslot_size(const ChunkStreamSlot* slot);

	/**
	 * returns the allocated size for this slot, when it's not a slot from the pool,
	 * it's always 0.
	 * @param slot the slot
	 * @return the size of the slot
	 */
	FREERDP_API size_t chunkstreamslot_allocated(const ChunkStreamSlot* slot);

	/**
	 * returns the data associated with this slot.
	 * @param slot the slot
	 * @return the memory buffer associated with this slot
	 */
	FREERDP_API BYTE* chunkstreamslot_data(const ChunkStreamSlot* slot);

	/**
	 * updates the used counter of this slot
	 * @param slot the slot
	 * @param used the new used size to set
	 * @return if the operation was successful (used can't be > to capacity)
	 */
	FREERDP_API BOOL chunkstreamslot_update_used(ChunkStreamSlot* slot, size_t used);

	/**
	 * updates the used counter of this slot by using the position in the stream
	 * @param slot the slot
	 * @param s a stream retrieved by chunkstream_getPoolStream()
	 * @return if the operation was successful (used can't be > to capacity)
	 */
	FREERDP_API BOOL chunkstreamslot_update_fromStream(ChunkStreamSlot* slot, wStream* s);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_CHUNKSTREAM_H */
