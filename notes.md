# General Notes

General outline
1. get handshake working and get a stream of data between two clients

# Protocol Description
These are kinda just ripped from the assignment but I think it makes it more understandable

## Peer Communication
- All operations are assumed to be implemented using TCP and interaction between peers is symmetrical (messages sent in both directions should look the same)
- Handshake must be done before any other message is sent

## Handshake Message

Parts of Handshake (32 Bytes Total)
1. Header    | 18 bytes | (`P2PFILESHARINGPROJ`)
2. zero bits | 10 bytes | 
3. peer ID   | 4 bytes  | ID of peer Client

## Actual Messages (After Handshake)
1. message Length | 4 byte | Length of message(does not include itself in length)
2. message Type   | 1 byte | specifies type of message (see below)
3. message payload| varies | 

### Message Types

#### choke
No payload

#### unchoke
No payload

#### interested
No payload

#### not interested
No payload

#### have
payload that contains 4-byte piece index field

#### bitfield
I am honestly confused by this right now

- Contains bitfield as its payload
- Each bit represents whether the peer has the corresponding bit
- Spare bits at the end are set to zero
- Only sent as first message right after hand shake
- peers that dont have anything yet can skip a bitfield message

first byte of bitfield: 0-7 (high bit to low bit)
second byte: 8-15
...

#### request
payload consists of a 4-byte piece index field.

#### piece
payload consists of a 4-byte peuice index field and the content of that peice (idk what this means yet)

# General Peer behaviour

## Connections
1. Peer A tries to make a TCP conn to Peer B
2. TCP connection is established
3. Peer A sends handshake to Peer B and recieves on from peer B
4. peer A checks handshake message
    - checks if peer B is the right neighber
    - check peer ID is the expected one
5. Peer A sends a bitfield message to let peer B know which file pieces it has
    - Peer B will also send its bitfield message to peer A
6. Peer A will look at the bitfield message from Peer B and if it has pieces that it doesnt have it will 
    send an interested message. Otherwise it will send not interested

## choke and unchoke
Choking is just restricting which other peers a client can send pieces to.

Choke => cant send pieces
Unchoked => can send pieces

- Each peer can upload pieces to at most k prefered neighbors (unchoke them) and 1 optimistically unchoked neighbor
    - k => given when program starts

- preferred neighbors are determined every p seconds

- preferred neighbors are selected by highest download rates during previous unchoking interval
    - ties are randomly broken

Example:
1. Peer A Selects Peer B as a preferred neighber (had a high download rate in prevoius unchoking interval)
2. If Peer B is not already unchoked then Peer A sends an unchoking message
3. Peer B sends peer A a request message (to request for pieces)
4. All other neighbors that are not unchoked should be choked via sending choke message (unless optimistic neighbor)

Note: if peer A has a complete file it determines preferred neighbors randomly among those interested in its data.

### Optimistic Unchoking
- optimistically unchoked neighbor is selected every m seconds ( m => unchoking interval)
- every m seconds a new neighbor among those interested in its data are unchoked

Their example situation
Suppose that peer C is randomly chosen as the optimistically unchoked neighbor of peer
A. Because peer A is sending data to peer C, peer A may become one of peer C’s
preferred neighbors, in which case peer C would start to send data to peer A. If the rate
at which peer C sends data to peer A is high enough, peer C could then, in turn, become
one of peer A’s preferred neighbors. Note that in this case, peer C may be a preferred
neighbor and optimistically unchoked neighbor at the same time. This kind of situation is
allowed. In the next optimistic unchoking interval, another peer will be selected as an
optimistically unchoked neighbor.

## Interested and not interested

- regardless of `choke` or `unchoke` if a neighbor has a piece a client wants then a peer sends a `interested` message to that neighbor

- each peer contains bitfields for all neighbros and updates them when it recieves `have` messages.
- no interesting pieces => `not interested`

## Request and Piece
peer A `unchoked` => sends `request` message to peer B for a random piece that peer B has and peer A doesnt

peer B gets `request` message => peer B sends `piece` message containing that piece

After peer A completely downloads the piece => peer A sends another `request`

... continues until peer A is choked by peer B

Notes:
- `request` message is sent ONLY after the peer recieves the piece message from prev `request`
- even though peer A `requests` a piece it may not always recieve the piece
    - due to it being choked



