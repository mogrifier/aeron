Aeron Archive
===

The aeron-archive is an service which enables Aeron data stream recording
and replay support from an archive. 

Currently implemented functionality:
- **Record:** service can record a particular subscription, described
by <__channel, streamId__>. Each resulting image for the subscription
will be recorded under a new __recordingId__. Only local Publications
are recorded, and so only IPC or spy subscriptions are supported, at
this time (this may change in near term future).

- **Replay:** service can replay a recorded __recordingId__ from
a particular __termId + termOffset__, and for a particular length.

- **Query:** service provides a rudimentary query interface which
allows __recordingId__ discovery and description. Currently this
supports a query for all descriptors or filtered by <__channel, streamId__>.

Usage
=====

Protocol
=====
Messages are specified using SBE in [aeron-archive-codecs.xml](https://github.com/real-logic/aeron/blob/master/aeron-archive/src/main/resources/aeron-archive-codecs.xml).
The Archive communicates via the following interfaces:
 - **Events channel:** other parties can subscribe to events for the start,
 stop, and progress of recordings. These are the
 recording events messages specified in the codec.
 
 - **Requests channel:** this allows clients to initiate replay or queries
 interactions with the archive. Requests have a correlationId sent
 on the initiating request. The `correlationId` is expected to be managed by
 the clients and is offered as a means for clients to track multiple
 concurrent requests. A request will typically involve the
 archive sending data back on the reply channel specified by the client 
 on the `ConnectRequest` message.

A control session can be established with the Archive. Operations happen within
the context of such a ControlSession which is allocated a `controlSessionId`.

Recording Events
----
Aeron clients wishing to observe the Archive recordings lifecycle can do so by
subscribing to the recording events channel. The messages are described in the codec.
To fully capture the state of the Archive a client could subscribe to these
events as well as query for the full list of descriptors.

Persisted Format
=====
The Archiver is backed by 2 file types, all of which are expected to reside in the __archiveDir__.

 -  **Catalog (one per archive):** The catalog contains fixed length (4k) records of recording
 descriptors. The descriptors can be queried as described above. Each descriptor entry is 4k aligned,
 and as the __recordingId__ is a simple sequence, this means lookup is a dead reckoning operation.
 Each entry has a frame (32b) followed by the RecordingDescriptor, the frame contains the encoded
 length of the RecordingDescriptor.
 See the codec for full descriptor details.
 
 - **Recording Segment Data (many per recorded stream):** This is where the recorded data is kept.
 Recording segments follow the naming convention of: __<recordingId>.<segmentIndex>.rec__
 The Archiver copies data as is from the recorded Image. As such the files follow the same convention
 as Aeron data streams. Data starts at __startPosition__, which translates into the offset
 __startPosition % termBufferLength__ in the first segment file. From there one can read fragments
 as described by the DataFragmentHeader up to the __stopPosition__. 
 
 
 