# AX08 Bridge Protocol

## Direction

There are two kinds of packets:
- **E2B**, that are packets send by the external client to the bridge.
- **B2E**, that are packets send by the bridge to the external client.

## Data Types

Data is always send in [Little Endian](https://en.wikipedia.org/wiki/Endianness) byte order.

Type|Size|Description
---|---|---
`int8`|1|Signed 8 bit integer.
`uint8`|1|Unsigned 8 bit integer.
`int16`|2|Signed 16 bit integer.
`uint16`|2|Unsigned 16 bit integer.
`int32`|4|Signed 32 bit integer.
`uint32`|4|Unsigned 32 bit integer.
`int64`|8|Signed 64 bit integer.
`uint64`|8|Unsigned 64 bit integer.
`char`|1|Single character
`array<size, type>`|`size`|`size` entries of `type` in a block, the element with the lowest index being send first.
`list8<type>`|`n`+1|Number of entires `n` as `uint8` folowed by the entries as `array<n, type>`.
`list16<type>`|`n`+1|Number of entires `n` as `uint16` folowed by the entries as `array<n, type>`.
`list32<type>`|`n`+1|Number of entires `n` as `uint32` folowed by the entries as `array<n, type>`.
`list64<type>`|`n`+1|Number of entires `n` as `uint64` folowed by the entries as `array<n, type>`.
`string8`|`n`+1|Alias for `list8<char>`
`string16`|`n`+2|Alias for `list16<char>`
`string32`|`n`+4|Alias for `list32<char>`
`string64`|`n`+8|Alias for `list64<char>`


## E2B Packets

### REQUEST STATUS PACKET

Requests the current status from the bridge. The bridge will respond with a [Bridge Status Packet](#bridge-status-packet).

Type|Value|Description
---|---|---
`uint8`|1|The packet id.

### UPLOAD PROGRAM PACKET

Uploads a program to the bridge. The bridge will respond with a [Program Info Packet](#program-info-packet).

Type|Value|Description
---|---|---
`uint8`|100|The packet id.
`string16`|`name`|The name of the program.
`array<4, uint64>`|`hash`|The hash of the program.
`array<65536, uint32>`|`instructions`|The instructions of program.

### DOWNLOAD PROGRAM PACKET

Requests the bridge to send back its current program. The bridge will respond with a [Program Send Packet](#program-send-packet).

Type|Value|Description
---|---|---
`uint8`|100|The packet id.
`string16`|`name`|The name of the program.
`array<65536, uint32>`|`instructions`|The instructions of program.

## B2E Packets

### BRIDGE INFO PACKET

Information about the bridge.

Type|Value|Description
---|---|---
`uint8`|1|The packet id
`uint16`|`protocol_version`|The protocol version.
`string8`|`version`|The version of the bridge firmware.

### PROGRAM INFO PACKET

Information about the currently running program.

Type|Value|Description
---|---|---
`uint8`|100|The packet id.
`string16`|`name`|The name of the program.
`array<4, uint64>`|`hash`|The hash of the program.

### PROGRAM SEND PACKET

Information about the currently running program.

Type|Value|Description
---|---|---
`uint8`|100|The packet id.
`string16`|`name`|The name of the program.
`array<4, uint64>`|`hash`|The hash of the program.
`array<65536, uint32>`|`instructions`|The instructions of program.
