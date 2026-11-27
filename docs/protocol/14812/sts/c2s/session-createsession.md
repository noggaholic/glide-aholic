# /Session/CreateSession

Build **14812** | `sts` | `c2s` | `/Session/CreateSession`

Envelope: [STS](../../../transport.md#sts-envelope).

## Notes

- Requests use P {path} STS/1.0 with l: body length and an optional s: sequence. See Transport for exact framing and limits.
- A successful /Sts/Connect must precede this request.
- Requires an active TLS connection under this profile. Missing TLS receives 403 Forbidden.
- Requires a logged-in active local session. Otherwise the response is 403 Forbidden. The request body is not inspected by this branch.
- Returns an empty Reply.
- A reply is sent only when the request includes a sequence. The request body is consumed in full in either case.
- XML values are found by exact tag spelling, reject duplicate opening tags, decode only the five named XML entities and require valid UTF-8. Other body fields are not established by this schema.

## Related messages

- [sts s2c session-createsession](../../../14812/sts/s2c/session-createsession.md) (reply): Response constructed when this request includes a sequence and its guards permit a reply.

## Referenced by

- [sts s2c session-createsession](../../../14812/sts/s2c/session-createsession.md) (reply): Request handled by this response branch.

[Build index](../../README.md)
