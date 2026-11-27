# /Sts/Ping

Build **49284** | `sts` | `c2s` | `/Sts/Ping`

Envelope: [STS](../../../transport.md#sts-envelope).

## Notes

- Requests use P {path} STS/1.0 with l: body length and an optional s: sequence. See Transport for exact framing and limits.
- A successful /Sts/Connect must precede this request.
- No response is sent and the body is not inspected.
- A reply is sent only when the request includes a sequence. The request body is consumed in full in either case.
- XML values are found by exact tag spelling, reject duplicate opening tags, decode only the five named XML entities and require valid UTF-8. Other body fields are not established by this schema.

[Build index](../../README.md)
