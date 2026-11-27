# /Sts/Connect

Build **49284** | `sts` | `c2s` | `/Sts/Connect`

Envelope: [STS](../../../transport.md#sts-envelope).

## XML body fields

| Tag | Required | Maximum decoded bytes | Meaning |
|---|:---:|---:|---|
| `Build` | Yes | 10 | Decimal build accepted by Server::BindSts. The STS SDK alias configured for this client is 1004; it differs from the executable build. |

## Notes

- Requests use P {path} STS/1.0 with l: body length and an optional s: sequence. See Transport for exact framing and limits.
- The first successful Connect binds the build and has no response. A rejected Build receives 400 Bad Request and the connection closes.
- A reply is sent only when the request includes a sequence. The request body is consumed in full in either case.
- XML values are found by exact tag spelling, reject duplicate opening tags, decode only the five named XML entities and require valid UTF-8. Other body fields are not established by this schema.

[Build index](../../README.md)
